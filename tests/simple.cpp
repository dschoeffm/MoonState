
#include <cassert>
#include <iostream>
#include <memory>

#include "samplePacket.hpp"
#include "stateMachine.hpp"

using namespace std;

class Identifier;
using SM = StateMachine<Identifier, SamplePacket>;

class Identifier {
public:
	struct ConnectionID {
		uint64_t val;

		bool operator==(const ConnectionID &c) const { return val == c.val; }

		bool operator<(const ConnectionID &c) const { return val < c.val; }

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
};

SamplePacket *getPkt() {
	uint32_t dataLen = 64;
	void *data = malloc(dataLen);
	SamplePacket *p = new SamplePacket(data, dataLen);
	return p;
}

void fun1(SM::State &state, SamplePacket *pktIn, SM::FunIface &fi) {
	cout << endl << "running fun1" << endl;

	if (state.state != 1) {
		cout << "fun1: state not 1" << endl;
	}

	if (*(reinterpret_cast<uint32_t *>(pktIn->getData())) == 2) {
		cout << "fun1: transision to state 2" << endl;
		state.transition(2);
		*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 12;
		fi.addPktToSend(pktIn);
		return;
	}

	if (*(reinterpret_cast<uint32_t *>(pktIn->getData())) == 3) {
		cout << "fun1: transision to state 3" << endl;
		state.transition(3);
		*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 12;
		fi.addPktToSend(pktIn);
		return;
	}

	cout << "fun1: no transision" << endl;

	*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 10;
	fi.addPktToSend(pktIn);
}

void fun2(SM::State &state, SamplePacket *pktIn, SM::FunIface &fi) {
	cout << endl << "running fun2" << endl;

	if (state.state != 2) {
		cout << "fun2: state not 2" << endl;
	}

	if (*(reinterpret_cast<uint32_t *>(pktIn->getData())) == 3) {
		cout << "fun2: transision to state 3" << endl;
		state.transition(3);
		*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 23;
		fi.addPktToSend(pktIn);
		return;
	}

	cout << "fun2: no transision" << endl;

	*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 24;
	fi.addPktToSend(pktIn);
}

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	try {

		SM sm;
		sm.registerStartStateID(1, nullptr);
		sm.registerEndStateID(3);
		sm.registerFunction(1, fun1);
		sm.registerFunction(2, fun2);

		assert(sm.getStateTableSize() == 0);

		vector<SamplePacket *> pktsIn;
		vector<SamplePacket *> pktsFree;
		vector<SamplePacket *> pktsSend;

		pktsIn.push_back(getPkt());

		*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) = 0;
		sm.runPktBatch(pktsIn, pktsSend, pktsFree);
		assert(*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) == 10);
		assert(sm.getStateTableSize() == 1);
		assert(pktsIn.size() == 1);
		assert(pktsSend.size() == 1);
		assert(pktsFree.size() == 0);
		pktsSend.clear();

		*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) = 2;
		sm.runPktBatch(pktsIn, pktsSend, pktsFree);
		assert(*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) == 12);
		assert(sm.getStateTableSize() == 1);
		assert(pktsIn.size() == 1);
		assert(pktsSend.size() == 1);
		assert(pktsFree.size() == 0);
		pktsSend.clear();

		*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) = 2;
		sm.runPktBatch(pktsIn, pktsSend, pktsFree);
		assert(*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) == 24);
		assert(sm.getStateTableSize() == 1);
		assert(pktsIn.size() == 1);
		assert(pktsSend.size() == 1);
		assert(pktsFree.size() == 0);
		pktsSend.clear();

		*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) = 3;
		sm.runPktBatch(pktsIn, pktsSend, pktsFree);
		assert(*(reinterpret_cast<uint32_t *>(pktsIn[0]->getData())) == 23);
		assert(sm.getStateTableSize() == 0);
		assert(pktsIn.size() == 1);
		assert(pktsSend.size() == 1);
		assert(pktsFree.size() == 0);
		pktsSend.clear();

	} catch (exception *e) {
		cout << endl << "FATAL:" << endl;
		cout << e->what() << endl;
	}
}
