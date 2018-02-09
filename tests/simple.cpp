#include <cassert>
#include <iostream>
#include <memory>

#include "samplePacket.hpp"
#include "stateMachine.hpp"

using namespace std;

class Identifier;
using SM = StateMachine<Identifier, SamplePacket>;

// This is a very elemental Identifier class
class Identifier {
public:
	struct ConnectionID {
		uint64_t val;

		bool operator==(const ConnectionID &c) const { return val == c.val; }

		bool operator<(const ConnectionID &c) const { return val < c.val; }

		operator std::string() const { return ""; }

		ConnectionID(const ConnectionID &c) : val(c.val){};
		ConnectionID() : val(0){};
	};

	struct Hasher {
		size_t operator()(const ConnectionID &id) const { return id.val; }
	};

	static ConnectionID identify(SamplePacket *pkt) {
		ConnectionID id;
		id.val = (uint64_t)pkt->getData();
		return id;
	};

	static ConnectionID getDelKey() {
		ConnectionID id;
		id.val = std::numeric_limits<uint64_t>::max();
		return id;
	};

	static ConnectionID getEmptyKey() {
		ConnectionID id;
		id.val = std::numeric_limits<uint64_t>::max() - 1;
		return id;
	};
};

// This just mallocs one SamplePacket
SamplePacket *getPkt() {
	uint32_t dataLen = 64;
	void *data = malloc(dataLen);
	SamplePacket *p = new SamplePacket(data, dataLen);
	return p;
}

// This is the function for state 1
void fun1(SM::State &state, SamplePacket *pktIn, SM::FunIface &fi) {

	// We don't actually need the FunIface
	(void)fi;
	cout << endl << "running fun1" << endl;

	// Sanity check -> this should never to true
	if (state.state != 1) {
		cout << "fun1: state not 1" << endl;
	}

	// if pktIn == 2 -> transition to state 2 and fill packet with 12
	if (*(reinterpret_cast<uint32_t *>(pktIn->getData())) == 2) {
		cout << "fun1: transision to state 2" << endl;
		fi.transition(2);
		*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 12;
		return;
	}

	// if pktIn == 3 -> transition to state 3 and fill packet with 13
	if (*(reinterpret_cast<uint32_t *>(pktIn->getData())) == 3) {
		cout << "fun1: transision to state 3" << endl;
		fi.transition(3);
		*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 13;
		return;
	}

	cout << "fun1: no transision" << endl;

	*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 10;
}

// This is the function for state 2
// Pretty much the same as state 1...
void fun2(SM::State &state, SamplePacket *pktIn, SM::FunIface &fi) {
	(void)fi;
	cout << endl << "running fun2" << endl;

	if (state.state != 2) {
		cout << "fun2: state not 2" << endl;
	}

	if (*(reinterpret_cast<uint32_t *>(pktIn->getData())) == 3) {
		cout << "fun2: transision to state 3" << endl;
		fi.transition(3);
		*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 23;
		return;
	}

	cout << "fun2: no transision" << endl;

	*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 24;
}

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	try {

		SM sm;

		// All incoming connections start at state 1, and don't require preparation
		sm.registerStartStateID(1, nullptr);

		// Every connection is state 3 should be deleted
		sm.registerEndStateID(3);

		// Register the functions to handle packets for these states
		sm.registerFunction(1, fun1);
		sm.registerFunction(2, fun2);

		// Setup sanity check
		assert(sm.getStateTableSize() == 0);

		// Prepare the packet array
		SamplePacket *sp = getPkt();
		SamplePacket **spArray = reinterpret_cast<SamplePacket **>(malloc(sizeof(void *)));
		spArray[0] = sp;

		// Prepare one BufArray
		BufArray<SamplePacket> pktsIn(spArray, 1);

		// Set data in input packet to 0
		*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) = 0;

		// Run the packet through the state machine
		sm.runPktBatch(pktsIn);

		// Check if the function misbehaved
		assert(*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) == 10);
		assert(sm.getStateTableSize() == 1);
		assert(pktsIn.getSendCount() == 1);
		assert(pktsIn.getFreeCount() == 0);

		// Reset the BufArray
		pktsIn.markSendPkt(0);

		// Same as above
		*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) = 2;
		sm.runPktBatch(pktsIn);
		assert(*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) == 12);
		assert(sm.getStateTableSize() == 1);
		assert(pktsIn.getSendCount() == 1);
		assert(pktsIn.getFreeCount() == 0);

		// Reset the BufArray
		pktsIn.markSendPkt(0);

		// Same as above
		*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) = 2;
		sm.runPktBatch(pktsIn);
		assert(*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) == 24);
		assert(sm.getStateTableSize() == 1);
		assert(pktsIn.getSendCount() == 1);
		assert(pktsIn.getFreeCount() == 0);

		// Reset the BufArray
		pktsIn.markSendPkt(0);

		// Same as above - This time the endstate is reached
		*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) = 3;
		sm.runPktBatch(pktsIn);
		assert(*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) == 23);
		assert(sm.getStateTableSize() == 0);
		assert(pktsIn.getSendCount() == 1);
		assert(pktsIn.getFreeCount() == 0);

		// Reset the BufArray
		pktsIn.markSendPkt(0);

	} catch (exception *e) {
		// Just catch whatever fails there may be
		cout << endl << "FATAL:" << endl;
		cout << e->what() << endl;

		return 1;
	}

	return 0;
}
