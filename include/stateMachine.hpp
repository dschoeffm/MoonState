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
	using StateID = uint16_t;
	using ConnectionID = typename Identifier::ConnectionID;
	using Hasher = typename Identifier::Hasher;

	static constexpr auto StateIDInvalid = std::numeric_limits<StateID>::max();

	struct State {
	public:
		void *stateData;
		StateID state;

		State(StateID state, void *stateData) : stateData(stateData), state(state) {};
		State(const State &s) : stateData(s.stateData), state(s.state) {};

		void transistion(StateID newState) { state = newState; }
	};

	class FunIface {
	private:
		StateMachine<Identifier, Packet> *sm;

	public:
		FunIface(StateMachine<Identifier, Packet> *sm) : sm(sm){};

		void addPktToFree(Packet *p) { sm->pktsToFree.push_back(p); }

		void addPktToSend(Packet *p) { sm->pktsToSend.push_back(p); }

		Packet *getPkt() {
			if (sm->pktsToFree.size() != 0) {
				Packet *ret = sm->pktsToFree.back();
				sm->pktsToFree.pop_back();
				return ret;
			} else if (sm->getPkt) {
				D(sm->getPktCBCounter++;)
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
	D(unsigned int getPktCBCounter = 0;)

	FunIface funIface;

	std::vector<Packet *> pktsToFree;
	std::vector<Packet *> pktsToSend;

	auto findState(ConnectionID id) {
	findStateLoop:
		auto stateIt = stateTable.find(id);
		if (stateIt == stateTable.end()) {
			// Add new state
			D(std::cout << "Adding new state" << std::endl;)
			State s(startStateID, nullptr);
			stateTable.insert({id, s});
			goto findStateLoop;
		} else {
			D(std::cout << "State found" << std::endl;)
		}
		return stateIt;
	};

	void runPkt(Packet *pktIn) {
		ConnectionID identity = identifier.identify(pktIn);

		auto stateIt = findState(identity);

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

	void addState(ConnectionID id, State st) {
		stateTable.insert({id, st});
		auto stateIt = findState(id);

		auto sfIt = functions.find(stateIt->second.state);
		if (sfIt == functions.end()) {
			throw std::runtime_error("StateMachine::addState() No such function found");
		}

		D(std::cout << "Running Function" << std::endl;)
		(sfIt->second)(stateIt->second, nullptr, funIface);

		if (stateIt->second.state == endStateID) {
			D(std::cout << "Reached endStateID - deleting connection" << std::endl;)
			removeState(id);
		}
	}

	void runPktBatch(std::vector<Packet *> &pktsIn, std::vector<Packet *> &pktsSend,
		std::vector<Packet *> &pktsFree) {
		for (auto pkt : pktsIn) {
			runPkt(pkt);
		}
#ifdef DEBUG
		// putting it all in D() and using assert would have been better...
		// for some reason that didn't work...
		if ((pktsToFree.size() + pktsToSend.size()) != (getPktCBCounter + pktsIn.size())) {
			std::cout << "pktsToFree: " << pktsToFree.size() << std::endl;
			std::cout << "pktsToSend: " << pktsToSend.size() << std::endl;
			std::cout << "pktsIn: " << pktsIn.size() << std::endl;
			std::cout << "getPktCBCounter: " << getPktCBCounter << std::endl;

			throw new std::runtime_error(
				"StateMachine::runPktBatch() some packets were are not in send/free vector");
		}
		getPktCBCounter = 0;
#endif
		pktsSend.clear();
		pktsFree.clear();
		std::swap(pktsSend, pktsToSend);
		std::swap(pktsFree, pktsToFree);
		pktsToSend.clear();
		pktsToFree.clear();
	}
};

#endif /* STATE_MACHINE_HPP */
