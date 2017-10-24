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

template <class Identifier, class Packet> class StateMachine {
public:
	struct State;
	class FunIface;

	using stateFun = std::function<void(State &, Packet *, FunIface &)>;
	using StateID = uint64_t;
	using ConnectionID = typename Identifier::ConnectionID;
	using Hasher = typename Identifier::Hasher;

	static constexpr auto StateIDInvalid = std::numeric_limits<StateID>::max();

	struct State {
	public:
		StateID state;
		void *stateData;

		State(StateID state, void *stateData) : state(state), stateData(stateData){};
		State(const State &s) : state(s.state), stateData(s.stateData){};

		void transistion(StateID newState) { state = newState; }
	};

	class FunIface {
	private:
		StateMachine<Identifier, Packet> *sm;

	public:
		FunIface(StateMachine<Identifier, Packet> *sm) : sm(sm){};

		void addPktToFree(Packet *p) { sm->pktsToFree.push_back(p); }

		void addPktToSend(Packet *p) { sm->pktsToSend.push_back(p); }

		Packet &getPkt() {
			if (sm->getPkt) {
				return sm->getPkt();
			} else {
				throw std::runtime_error(
					"StateMachine::SMFunIface::getPkt no function registered");
			}
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

	std::function<Packet &()> getPktCB;

	FunIface funIface;

	std::vector<Packet *> pktsToFree;
	std::vector<Packet *> pktsToSend;

	void runPkt(Packet *pktIn) {
		ConnectionID identity = identifier.identify(pktIn);

	findState:
		auto stateIt = stateTable.find(identity);
		if (stateIt == stateTable.end()) {
			// Add new state
			D(std::cout << "Adding new state" << std::endl;)
			State s(startStateID, nullptr);
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
		(sfIt->second)(stateIt->second, pktIn, funIface);

		if (stateIt->second.state == endStateID) {
			D(std::cout << "Reached endStateID - deleting connection" << std::endl;)
			removeState(identity);
		}
	}


public:
	StateMachine() : startStateID(0), endStateID(StateIDInvalid), funIface(this){};

	size_t getStateTableSize() { return stateTable.size(); };

	void registerFunction(StateID id, stateFun function) { functions.insert({id, function}); }

	void registerEndStateID(StateID endStateID) { this->endStateID = endStateID; }
	void registerStartStateID(StateID startStateID) { this->startStateID = startStateID; }

	void registerGetPktCB(std::function<Packet &()> fun) { getPktCB = fun; }

	void removeState(ConnectionID id) { stateTable.erase(id); }

	void runPktBatch(std::vector<Packet *> &pktsIn, std::vector<Packet *> &pktsSend,
		std::vector<Packet *> &pktsFree) {
		for(auto pkt : pktsIn){
			runPkt(pkt);
			std::swap(pktsSend, pktsToSend);
			std::swap(pktsFree, pktsToFree);
			pktsToSend.clear();
			pktsToFree.clear();
		}
	}
};

#endif /* STATE_MACHINE_HPP */
