#ifndef STATE_MACHINE_HPP
#define STATE_MACHINE_HPP

#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include "common.hpp"
#include "exceptions.hpp"
#include "bufArray.hpp"

template <class Identifier, class Packet> class StateMachine {
public:
	struct State;
	class FunIface;

	using stateFun = std::function<void(State &, Packet *, FunIface &)>;
	using ConnectionID = typename Identifier::ConnectionID;
	using Hasher = typename Identifier::Hasher;

	static constexpr auto StateIDInvalid = std::numeric_limits<StateID>::max();

	struct State {
	public:
		void *stateData;
		StateID state;

		State(StateID state, void *stateData) : stateData(stateData), state(state) {};
		State(const State &s) : stateData(s.stateData), state(s.state) {};

		void transition(StateID newState) { state = newState; }
	};

	class FunIface {
	private:
		StateMachine<Identifier, Packet> *sm;
		Packet* pkt;
		BufArray<Packet>& pktsSend;
		BufArray<Packet>& pktsFree;
		bool sendPkt;

	public:
		FunIface(StateMachine<Identifier, Packet> *sm, Packet* pkt, BufArray<Packet>& pktsSend, BufArray<Packet>& pktsFree) : sm(sm), pkt(pkt), pktsSend(pktsSend), pktsFree(pktsFree), sendPkt(true){};

		~FunIface(){
			if(sendPkt){
				pktsSend.addPkt(pkt);
			} else {
				pktsFree.addPkt(pkt);
			}
		}

		void freePkt() { sendPkt = false; }

#if 0
		Packet *getPkt() {
			if (sm->pktsToFree.size() != 0) {
				Packet *ret = sm->pktsToFree.back();
				sm->pktsToFree.pop_back();
				return ret;
			} else if (sm->getPktCB) {
				D(sm->getPktCBCounter++;)
				return sm->getPktCB();
			} else {
				throw std::runtime_error(
					"StateMachine::SMFunIface::getPkt no function registered");
			}
		}
#endif

		Packet *getPkt() {
			throw new std::runtime_error(
				"StateMachine::FunIface::getPkt() not implemented");
		}

		void setTimeout() {
			throw new std::runtime_error(
				"StateMachine::FunIface::setTimeout not implemented");
		}
	};

private:
	std::unordered_map<ConnectionID, State, Hasher> stateTable;

	std::unordered_map<StateID, stateFun> functions;

	Identifier identifier;

	StateID startStateID;
	StateID endStateID;
	std::function<void*(ConnectionID)> startStateFun;

	std::function<Packet *()> getPktCB;
	D(unsigned int getPktCBCounter = 0;)

	auto findState(ConnectionID id) {
	findStateLoop:
		auto stateIt = stateTable.find(id);
		if (stateIt == stateTable.end()) {
			// Add new state
			D(std::cout << "Adding new state" << std::endl;)
			D(std::cout << "ConnectionID: " << static_cast<std::string>(id) << std::endl;)

			// Create startState data object
			void* stateData = nullptr;
			if(startStateFun){
				stateData = startStateFun(id);
			}

			State s(startStateID, stateData);
			stateTable.insert({id, s});
			goto findStateLoop;
		} else {
			D(std::cout << "State found" << std::endl;)
		}
		return stateIt;
	};

	void runPkt(Packet *pktIn, BufArray<Packet>& pktsSend, BufArray<Packet>& pktsFree) {
		try {
			ConnectionID identity = identifier.identify(pktIn);

			auto stateIt = findState(identity);

			// TODO unregister timeout, if one exists

			auto sfIt = functions.find(stateIt->second.state);
			if (sfIt == functions.end()) {
				throw std::runtime_error("StateMachine::runPkt() No such function found");
			}

			FunIface funIface(this, pktIn, pktsSend, pktsFree);

			D(std::cout << "Running Function" << std::endl;)
			(sfIt->second)(stateIt->second, pktIn, funIface);

			if (stateIt->second.state == endStateID) {
				D(std::cout << "Reached endStateID - deleting connection" << std::endl;)
				removeState(identity);
			}
		} catch (PacketNotIdentified *e) {
			pktsFree.addPkt(pktIn);
		}
	}

public:
	StateMachine() : startStateID(0), endStateID(StateIDInvalid) {};

	size_t getStateTableSize() { return stateTable.size(); };

	void registerFunction(StateID id, stateFun function) { functions.insert({id, function}); }

	void registerEndStateID(StateID endStateID) { this->endStateID = endStateID; }
	void registerStartStateID(
		StateID startStateID, std::function<void *(ConnectionID)> startStateFun) {
		this->startStateID = startStateID;
		this->startStateFun = startStateFun;
	}

	void registerGetPktCB(std::function<Packet *()> fun) { getPktCB = fun; }

	void removeState(ConnectionID id) { stateTable.erase(id); }

	void addState(ConnectionID id, State st, Packet* pkt) {
		stateTable.insert({id, st});
		auto stateIt = findState(id);

		auto sfIt = functions.find(stateIt->second.state);
		if (sfIt == functions.end()) {
			throw std::runtime_error("StateMachine::addState() No such function found");
		}

		FunIface funIface(this, pkt, nullptr, nullptr);

		D(std::cout << "Running Function" << std::endl;)
		(sfIt->second)(stateIt->second, pkt, funIface);

		if (stateIt->second.state == endStateID) {
			D(std::cout << "Reached endStateID - deleting connection" << std::endl;)
			removeState(id);
		}
	}

	void runPktBatch(BufArray<Packet> &pktsIn, BufArray<Packet> &pktsSend,
		BufArray<Packet> &pktsFree) {
		for (auto pkt : pktsIn) {
			runPkt(pkt, pktsSend, pktsFree);
		}
	}
};

#endif /* STATE_MACHINE_HPP */
