#ifndef STATE_MACHINE_HPP
#define STATE_MACHINE_HPP

#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>

#include "common.hpp"
#include "packet.hpp"

template <class Identifier> class StateMachine {
public:
	struct State;

	using stateFun = std::function<uint32_t(State &, Packet &, Packet &)>;
	using StateID = uint64_t;
	using ConnectionID = typename Identifier::ConnectionID;
	using Hasher = typename Identifier::Hasher;

	static constexpr auto StateIDInvalid = std::numeric_limits<StateID>::max();

	struct State {
	public:
		StateID state;
		void *stateData;
		StateMachine<Identifier> *machine;

		State(StateID state, void *stateData, StateMachine<Identifier> *machine)
			: state(state), stateData(stateData), machine(machine){};
		State(const State &s) : state(s.state), stateData(s.stateData), machine(s.machine){};

		void transistion(StateID newState) { state = newState; }

		// After mSec microseconds, transition to newState
		void registerTimeout(unsigned int mSec, StateID newState) {
			machine->registerTimeout(mSec, newState);
		}
	};

private:
	std::unordered_map<ConnectionID, State, Hasher> stateTable;

	std::unordered_map<StateID, stateFun> functions;

	Identifier identifier;

	StateID startStateID;
	StateID endStateID;

public:
	StateMachine() : startStateID(0), endStateID(StateIDInvalid){};

	size_t getStateTableSize() { return stateTable.size(); };

	void registerFunction(StateID id, stateFun function) { functions.insert({id, function}); }

	void registerEndStateID(StateID endStateID) { this->endStateID = endStateID; }
	void registerStartStateID(StateID startStateID) { this->startStateID = startStateID; }

	void removeState(ConnectionID id) { stateTable.erase(id); }

	uint32_t runPkt(Packet pktIn, Packet pktOut) {
		ConnectionID identity = identifier.identify(pktIn);

findState:
		auto stateIt = stateTable.find(identity);
		if (stateIt == stateTable.end()) {
			// Add new state
			D(std::cout << "Adding new state" << std::endl;)
			State s(startStateID, nullptr, this);
			stateTable.insert({identity, s});
			goto findState;
		} else {
			D(std::cout << "State found" << std::endl;)
		}

		// TODO unregister timeout, if one exists

		auto sfIt = functions.find(stateIt->second.state);
		if (sfIt == functions.end()) {
			throw std::runtime_error("StateMachine::runPkt() No such function found");
		}

		D(std::cout << "Running Function" << std::endl;)
		uint32_t ret = (sfIt->second)(stateIt->second, pktIn, pktOut);

		if (stateIt->second.state == endStateID) {
			D(std::cout << "Reached endStateID - deleting connection" << std::endl;)
			removeState(identity);
		}

		return ret;
	}
};

#endif /* STATE_MACHINE_HPP */
