
#include <iostream>
#include <cassert>

#include "packet.hpp"
#include "stateMachine.hpp"

using namespace std;

class Identifier {
public:
	struct ConnectionID {
		uint64_t val;

		bool operator==(const ConnectionID &c) const {
			return val == c.val;
		}

		bool operator<(const ConnectionID &c) const {
			return val < c.val;
		}

		ConnectionID(const ConnectionID& c) : val(c.val) {};
		ConnectionID() : val(0) {};
	};

	struct Hasher {
		size_t operator()(const ConnectionID& id) const {
			return id.val;
		}
	};

	static ConnectionID identify(Packet pkt){
		ConnectionID id;
		id.val = (uint64_t) pkt.data;
		return id;
	};
};

uint32_t fun1(StateMachine<Identifier>::State &state, Packet pktIn, Packet pktOut) {
	(void)pktOut;

	if (state.state != 1) {
		cout << "fun1: state not 1" << endl;
	}

	if (pktIn.dataLen == 2) {
		state.transistion(2);
		return 1;
	}

	if (pktIn.dataLen == 3) {
		state.transistion(3);
		return 2;
	}

	return 0;
}

uint32_t fun2(StateMachine<Identifier>::State &state, Packet pktIn, Packet pktOut) {
	(void)pktOut;

	if (state.state != 2) {
		cout << "fun2: state not 2" << endl;
	}

	if (pktIn.dataLen == 3) {
		state.transistion(3);
		return 3;
	}

	return 4;
}

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	StateMachine<Identifier> sm;
	sm.registerStartStateID(1);
	sm.registerEndStateID(3);
	sm.registerFunction(1, fun1);
	sm.registerFunction(2, fun2);

	assert(sm.getStateTableSize() == 0);

	Packet p1;
	p1.data = (void *) 0x100;

	p1.dataLen = 0;
	assert(sm.runPkt(p1, p1) == 0);
	assert(sm.getStateTableSize() == 1);

	p1.dataLen = 1;
	assert(sm.runPkt(p1, p1) == 0);
	assert(sm.getStateTableSize() == 1);

	p1.dataLen = 2;
	assert(sm.runPkt(p1, p1) == 1);
	assert(sm.getStateTableSize() == 1);

	p1.dataLen = 2;
	assert(sm.runPkt(p1, p1) == 4);
	assert(sm.getStateTableSize() == 1);

	p1.dataLen = 3;
	assert(sm.runPkt(p1, p1) == 3);
	assert(sm.getStateTableSize() == 0);

}
