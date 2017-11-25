#ifndef STATE_MACHINE_HPP
#define STATE_MACHINE_HPP

#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "bufArray.hpp"
#include "common.hpp"
#include "exceptions.hpp"
#include "spinlock.hpp"

/*! State machine framework
 *
 * This class is a comprehensive framework, on top of which a developer can
 * build high-performance state machines.
 *
 * It can be specialized to work with different underlying packet packet sources
 * and packet identifiers.
 *
 * The Identifier needs to have the following form:
 * \code{.cpp}
 * class Identifier {
 * 		// This class needs to be unique for each packet
 *		class ConnectionID {
 *			bool operator==(const ConnectionID &c) const;
 *			bool operator<(const ConnectionID &c) const;
 *			operator std::string() const;
 *			ConnectionID(const ConnectionID &c);
 *			ConnectionID();
 *		};
 *
 *		struct Hasher {
 *			// This should be uniformly distributed
 *			// Collisions can happen, but are not great
 *			size_t operator()(const ConnectionID &c) const;
 *		};
 *
 *		static ConnectionID identify(Packet *pkt);
 * };
 * \endcode
 *
 * The Packet needs to have the following form:
 * \code{.cpp}
 * struct Packet {
 *		void *getData();
 *		uint16_t getDataLen();
 *		void setDataLen(uint16_t l);
 *		uint16_t getBufLen();
 * };
 * \endcode
 *
 * \tparam Identifier This class is used to uniquely identify incoming packets (see above)
 * \tparam Packet This class wraps around any kind of packet buffer (see above)
 */

template <class Identifier, class Packet> class StateMachine {
private:
	using ConnectionID = typename Identifier::ConnectionID;
	using Hasher = typename Identifier::Hasher;
	static constexpr auto timeoutIDInvalid = std::numeric_limits<uint32_t>::max();

public:
	/*
	 * XXX -------------------------------------------- XXX
	 *       Public declarations
	 * XXX -------------------------------------------- XXX
	 */

	struct State;
	class FunIface;

	/*! This is the signature any state function needs to expose */
	using stateFun = std::function<void(State &, Packet *, FunIface &)>;

	/*! This is the signature any timeout function needs to expose */
	using timeoutFun = std::function<void(State &, FunIface &)>;

	/*! Represents an invalid StateID */
	static constexpr auto StateIDInvalid = std::numeric_limits<StateID>::max();

	/*! Represents one connection
	 *
	 * This struct holds the information about the current state, and a void*
	 * which points to any kind of data the user chooses
	 */
	struct State {
	public:
		void *stateData;
		StateID state;
		uint32_t timeoutID;

		State(StateID state, void *stateData)
			: stateData(stateData), state(state), timeoutID(timeoutIDInvalid){};
		State(const State &s) : stateData(s.stateData), state(s.state){};

		/*! Transition to another state
		 * \param newState The state to transition to
		 */
		void transition(StateID newState) { state = newState; }
	};

	/*
	 * XXX -------------------------------------------- XXX
	 *       Interface exposed to state functions
	 * XXX -------------------------------------------- XXX
	 */

	/*! Main interface for the needs of a state function
	 */
	class FunIface {
	private:
		friend class StateMachine<Identifier, Packet>;

		StateMachine<Identifier, Packet> *sm;
		uint32_t pktIdx;
		BufArray<Packet> &pktsBA;
		ConnectionID &cID;
		State &state;
		bool sendPkt;

		// Private -> nobody can misuse any FunIface objects
		FunIface(StateMachine<Identifier, Packet> *sm, uint32_t pktIdx,
			BufArray<Packet> &pktsBA, ConnectionID &cID, State &state)
			: sm(sm), pktIdx(pktIdx), pktsBA(pktsBA), cID(cID), state(state), sendPkt(true){};

	public:
		~FunIface() {
			if (!sendPkt) {
				pktsBA.markDropPkt(pktIdx);
			}
		}

		/*! Free the packets after the batch is processed, do not send it */
		void freePkt() { sendPkt = false; }

		/*! Get an additional packet buffer
		 * \return The new packet buffer
		 */
		Packet *getPkt() {
			throw new std::runtime_error("StateMachine::FunIface::getPkt() not implemented");
		}

		/*! Set a timeout, after which a transition will happen
		 * \param timeout Time in milliseconds, until timeout will occur
		 * \param fun Function to execute, if timeout occurs
		 */
		void setTimeout(std::chrono::milliseconds timeout, timeoutFun fun) {
			std::chrono::time_point<std::chrono::steady_clock> now =
				std::chrono::steady_clock::now();
			std::chrono::time_point<std::chrono::steady_clock> then = now + timeout;

			struct Timeout t;
			t.time = then;
			t.timeoutID = sm->curTimeoutID++;

			state.timeoutID = t.timeoutID;

			sm->timeoutsQ.push(t);

			// This is an alternative to emplace
			// emplace should be better
			/*
			auto td = std::make_unique<struct TimeoutData>(cID, fun);
			sm->timeoutFunctions.insert(
				std::pair<uint32_t, std::unique_ptr<struct TimeoutData>>(
					t.timeoutID, std::move(td)));
			*/

			sm->timeoutFunctions.emplace(
				t.timeoutID, std::make_unique<struct TimeoutData>(cID, fun));
		}
	};

private:
	/*
	 * XXX -------------------------------------------- XXX
	 *       Private attributes
	 * XXX -------------------------------------------- XXX
	 */

	// This is the heart of the state tracking
	// stateTable holds the link between connections and states
	std::unordered_map<ConnectionID, State, Hasher> stateTable;

	// This table specifies which function should be called for a packet
	// belonging to a connection in a specific state
	std::unordered_map<StateID, stateFun> functions;

	// TODO: Since the identifier should not track anything, maybe we don't
	// even need a member -> all static functions
	Identifier identifier;

	// The state a newly received connection starts in
	// This is only useful, if listenToConnections is true
	StateID startStateID;

	// This function gets called on newly received connections
	// This is only useful, if listenToConnections is true
	std::function<void *(ConnectionID)> startStateFun;

	// If a connection reaches this state, it gets destryed
	StateID endStateID;

	// Callback to aquire new packets
	std::function<Packet *()> getPktCB;

	// Basically: Server mode or client mode
	bool listenToConnections;

	/*
	 * XXX -------------------------------------------- XXX
	 *       Timeout handling
	 * XXX -------------------------------------------- XXX
	 */

	// This represents the timeout tracking
	struct Timeout {
		// This is the, when the timeout ticks out
		std::chrono::time_point<std::chrono::steady_clock> time;

		// This uniquely identifies a single timeout
		uint32_t timeoutID;

		// Make sure, that the nearest timeout is the first one in timeoutsQ (see below)
		class Compare {
		public:
			bool operator()(struct Timeout a, struct Timeout b) { return a.time < b.time; };
		};
	};

	// This increments for every timeout -> provides a unique ID
	uint32_t curTimeoutID;

	// This contains all the timeouts
	std::priority_queue<struct Timeout, std::vector<struct Timeout>,
		typename Timeout::Compare>
		timeoutsQ;

	// This is used only to check if a timeout is valid, and given that, to
	// execute it
	struct TimeoutData {
		// This is used to identify, which connection the timeout belongs to
		// indexes the stateTable
		ConnectionID id;

		// This function is executed, if the timeout ticks out
		timeoutFun fun;

		// Trivial constructor
		TimeoutData(ConnectionID id, timeoutFun fun) : id(id), fun(fun){};
	};

	// If a timeoutID maps to a TimeoutData, this timeout is valid
	// if it maps to ::end(), then it is ignored.
	// The unique_ptr should make sure, that the map is not blown up by large ConnectionIDs
	// XXX The indirection assumes, that timeouts don't happen frequently, otherwise
	// it may be better to store them directly in the map? (not sure)
	std::unordered_map<uint32_t, std::unique_ptr<struct TimeoutData>> timeoutFunctions;

	/*
	 * XXX -------------------------------------------- XXX
	 *       Connection sharing
	 * XXX -------------------------------------------- XXX
	 */

	// These two members allow to open a connection on one core,
	// and receive subsequent packets on another
	static SpinLockCLSize newStatesLock;
	static std::unordered_map<ConnectionID, State, Hasher> newStates;

	/*
	 * XXX -------------------------------------------- XXX
	 *       Private helper methods
	 * XXX -------------------------------------------- XXX
	 */

	auto findState(ConnectionID id) {
	findStateLoop:
		auto stateIt = stateTable.find(id);
		if (stateIt == stateTable.end()) {

			// Check, if this is a connection opened by another core
			{
				std::lock_guard<SpinLockCLSize> guard(newStatesLock);
				auto maybeStateIt = newStates.find(id);
				if (maybeStateIt != newStates.end()) {
					// Found a suitable state, insert it into own stateTable
					stateTable.insert(*maybeStateIt);
					newStates.erase(maybeStateIt);
					goto findStateLoop;
				}
			}

			// Maybe accept the new connection
			if (listenToConnections) {
				// Add new state
				D(std::cout << "Adding new state" << std::endl;)
				D(std::cout << "ConnectionID: " << static_cast<std::string>(id) << std::endl;)

				// Create startState data object
				void *stateData = nullptr;
				if (startStateFun) {
					stateData = startStateFun(id);
				}

				State s(startStateID, stateData);
				stateTable.insert({id, s});
				goto findStateLoop;
			}

		} else {
			D(std::cout << "State found" << std::endl;)
		}
		return stateIt;
	};

	void runPkt(BufArray<Packet> &pktsIn, unsigned int cur) {
		D(std::cout << "StateMachine::runPkt() called" << std::endl;)

		try {
			Packet *pktIn = pktsIn[cur];
			ConnectionID identity = identifier.identify(pktIn);

			auto stateIt = findState(identity);

			if (stateIt == stateTable.end()) {
				// We don't want this packet
				D(std::cout << "StateMachine::runPkt() discarding packet" << std::endl;)
				D(std::cout << "ident of packet: " << static_cast<std::string>(identity)
							<< std::endl;)
				return;
			}

			if (stateIt->second.timeoutID != timeoutIDInvalid) {
				this->timeoutFunctions.erase(stateIt->second.timeoutID);
				stateIt->second.timeoutID = timeoutIDInvalid;
			}

			auto sfIt = functions.find(stateIt->second.state);
			if (sfIt == functions.end()) {
				D(std::cout << "StateMachine::runPkt() Didn't find a function for this state"
							<< std::endl;)
				throw std::runtime_error("StateMachine::runPkt() No such function found");
			}

			FunIface funIface(this, cur, pktsIn, identity, stateIt->second);

			D(std::cout << "Running Function" << std::endl;)
			(sfIt->second)(stateIt->second, pktIn, funIface);

			if (stateIt->second.state == endStateID) {
				D(std::cout << "Reached endStateID - deleting connection" << std::endl;)
				removeState(identity);
			}
		} catch (PacketNotIdentified *e) {
			D(std::cout << "StateMachine::runPkt() Packet could not be identified"
						<< std::endl;);
			pktsIn.markDropPkt(cur);
		}
	}

public:
	/*
	 * XXX -------------------------------------------- XXX
	 *       Public interface
	 * XXX -------------------------------------------- XXX
	 */

	StateMachine()
		: startStateID(0), endStateID(StateIDInvalid), listenToConnections(false),
		  curTimeoutID(0){};

	/*! Get the number of tracked connections
	 * This is probably only for statistics
	 *
	 * \return Number of connections
	 */
	size_t getStateTableSize() { return stateTable.size(); };

	/*! Register a function for a given state
	 *
	 * This function should be called once for each state you wish to use
	 *
	 * \param id ID of the state for which to set a function
	 * \param function This function will be called, when a connection is in
	 * 		state id, and gets a packet
	 */
	void registerFunction(StateID id, stateFun function) { functions.insert({id, function}); }

	/*! This registers an end state
	 * If a connections reaches this id, it will be destroyed
	 *
	 * \param endStateID the state in question
	 */
	void registerEndStateID(StateID endStateID) { this->endStateID = endStateID; }

	/*! This method describes, how to proceed with incoming connections
	 *
	 * If you call this function, you allow for incoming connections.
	 * They will start using the parameters.
	 *
	 * \param startStateID All new connections will start in this state
	 * \param startStateFun This function is called to populate the void* for the state
	 * 		(may be nullptr)
	 */
	void registerStartStateID(
		StateID startStateID, std::function<void *(ConnectionID)> startStateFun) {
		this->startStateID = startStateID;
		this->startStateFun = startStateFun;
		listenToConnections = true;
	}

	/*! Register a callback in order to get new buffer
	 *
	 * \param fun Function to call, if new buffers are needed
	 */
	void registerGetPktCB(std::function<Packet *()> fun) { getPktCB = fun; }

	/*! Remove a connection
	 *
	 * Using this function you can delete a connection.
	 *
	 * \param id The connection id of the connection to remove
	 */
	void removeState(ConnectionID id) { stateTable.erase(id); }

	/*! Open an outgoing connection
	 *
	 * This is the function you want to call, if you want to connect to a server.
	 *
	 * \param id The connection id this connection will use
	 * \param st The state data
	 * \param pktsIn Packet buffer for the state to work with (only one packet)
	 * \param pktsSend Packets to send out (same as runPktBatch())
	 * \param pktsFree Packets to free (same as runPktBatch())
	 */
	void addState(ConnectionID id, State st, BufArray<Packet> &pktsIn) {

		auto sfIt = functions.find(st.state);
		if (sfIt == functions.end()) {
			throw std::runtime_error("StateMachine::addState() No such function found");
		}

		FunIface funIface(this, 0, pktsIn, id, st);

		D(std::cout << "StateMachine::addState() Running Function" << std::endl;)
		(sfIt->second)(st, pktsIn[0], funIface);

		if (st.state == endStateID) {
			D(std::cout << "StateMachine::addState() Reached endStateID - deleting connection"
						<< std::endl;)
			return;
		}

		{
			D(std::cout << "StateMachine::addState() adding connection to newStates"
						<< std::endl;)
			D(std::cout << "StateMachine::addState() identity: "
						<< static_cast<std::string>(id) << std::endl;)
			std::lock_guard<SpinLockCLSize> lock(newStatesLock);
			newStates.insert({id, st});
		}
	}

	/*! Run a batch of packets
	 *
	 * This method is the function you want to call, in order to pump new packets
	 * into the state machine
	 *
	 * \param pktsIn Incoming packets
	 * \param pktsSend Packets to be sent, after the method returns
	 * 		(caller has to make sure, there is enough room)
	 * \param pktsFree Packets to be freed, after the method returns
	 * 		(caller has to make sure, there is enough room)
	 */
	void runPktBatch(BufArray<Packet> &pktsIn) {
		uint32_t inCount = pktsIn.getTotalCount();

		// This loop handles the timeouts
		// It breaks, if there are no usable timeouts anymore
		// It will (usually) not run until timeoutsQ is empty
		while (!timeoutsQ.empty()) {
			// Get the next timeout
			auto timeoutElem = timeoutsQ.top();

			// If the current timeout is in the future -> break
			if (timeoutElem.time > std::chrono::steady_clock::now()) {
				break;
			}

			// Extract some info from the timeout
			uint32_t timeoutID = timeoutElem.timeoutID;
			auto timeoutDataIt = timeoutFunctions.find(timeoutID);

			// Check if the timeout is valid
			if (timeoutDataIt == timeoutFunctions.end()) {
				timeoutsQ.pop();
				continue;
			}

			// Prepare function call
			std::unique_ptr<struct TimeoutData> timeoutData =
				std::move(timeoutDataIt->second);
			auto stateIt = findState(timeoutData->id);
			assert(stateIt != stateTable.end());
			FunIface funIface(this, std::numeric_limits<uint32_t>::max(), pktsIn,
				timeoutData->id, stateIt->second);

			// Clear the timeoutID from the state
			stateIt->second.timeoutID = timeoutIDInvalid;

			// Call function
			timeoutData->fun(stateIt->second, funIface);

			// Check if we reached the end state
			if (stateIt->second.state == endStateID) {
				D(std::cout << "Reached endStateID - deleting connection" << std::endl;)
				removeState(timeoutData->id);
			}

			// Clean up
			timeoutsQ.pop();
			timeoutFunctions.erase(timeoutDataIt);
		}

		// Run all the usual incoming packets
		for (uint32_t i = 0; i < inCount; i++) {
			runPkt(pktsIn, i);
		}
	}
};

// Define static members of the state machine

// Don't try to understand the template stuff, it works...
template <class Identifier, class Packet>
std::unordered_map<typename Identifier::ConnectionID,
	typename StateMachine<Identifier, Packet>::State, typename Identifier::Hasher>
	StateMachine<Identifier, Packet>::newStates;

template <class Identifier, class Packet>
SpinLockCLSize StateMachine<Identifier, Packet>::newStatesLock;

#endif /* STATE_MACHINE_HPP */
