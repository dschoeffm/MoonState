#include <cassert>
#include <iostream>
#include <limits>
#include <memory>

#include "sodium.h"

#include "samplePacket.hpp"
#include "stateMachine.hpp"

union tsc_t {
	uint64_t tsc_64;
	struct {
		uint32_t lo_32;
		uint32_t hi_32;
	};
};

inline static uint64_t read_rdtsc() {
	union tsc_t tsc;
	asm volatile("RDTSC\n\t"
				 "mov %%edx, %0\n\t"
				 "mov %%eax, %1\n\t"
				 : "=r"(tsc.hi_32), "=r"(tsc.lo_32)::"%rax", "%rbx", "%rcx", "%rdx");
	return tsc.tsc_64;
}

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
		ConnectionID(uint64_t val) : val(val){};
	};

	static struct ConnectionID getDelKey() {
		return ConnectionID(std::numeric_limits<uint64_t>::max());
	};

	static struct ConnectionID getEmptyKey() {
		return ConnectionID(std::numeric_limits<uint64_t>::max() - 1);
	};

	struct Hasher {
		size_t operator()(const ConnectionID &id) const {
			uint64_t res;
			assert(crypto_shorthash_BYTES == 8);
			uint8_t key[crypto_shorthash_KEYBYTES];
			memset(key, 0, crypto_shorthash_KEYBYTES);
			crypto_shorthash(reinterpret_cast<uint8_t *>(&res),
				reinterpret_cast<const uint8_t *>(&id.val), sizeof(id.val), key);

			return res;
		}
	};

	static ConnectionID identify(SamplePacket *pkt) {
		ConnectionID id;
		id.val = (uint64_t)pkt->getData();
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

	DEBUG_ENABLED(cout << endl << "running fun1" << endl;)

	// Sanity check -> this should never to true
	DEBUG_ENABLED(if (state.state != 1) { cout << "fun1: state not 1" << endl; })

	// if pktIn == 2 -> transition to state 2 and fill packet with 12
	if (*(reinterpret_cast<uint32_t *>(pktIn->getData())) == 2) {
		DEBUG_ENABLED(cout << "fun1: transision to state 2" << endl;)
		fi.transition(2);
		*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 12;
		return;
	}

	// if pktIn == 3 -> transition to state 3 and fill packet with 13
	if (*(reinterpret_cast<uint32_t *>(pktIn->getData())) == 3) {
		DEBUG_ENABLED(cout << "fun1: transision to state 3" << endl;)
		fi.transition(3);
		*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 13;
		return;
	}

	DEBUG_ENABLED(cout << "fun1: no transision" << endl;)

	*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 10;
}

// This is the function for state 2
// Pretty much the same as state 1...
void fun2(SM::State &state, SamplePacket *pktIn, SM::FunIface &fi) {
	DEBUG_ENABLED(cout << endl << "running fun2" << endl;)

	DEBUG_ENABLED(if (state.state != 2) { cout << "fun2: state not 2" << endl; })

	if (*(reinterpret_cast<uint32_t *>(pktIn->getData())) == 3) {
		DEBUG_ENABLED(cout << "fun2: transision to state 3" << endl;)
		fi.transition(3);
		*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 23;
		return;
	}

	DEBUG_ENABLED(cout << "fun2: no transision" << endl;)

	*(reinterpret_cast<uint32_t *>(pktIn->getData())) = 24;
}

void usage(std::string progName) {
	std::cout << "Usage: " << progName << " <Num States>" << std::endl;
	std::exit(0);
}

int main(int argc, char **argv) {

	if (argc < 2) {
		usage(std::string(argv[0]));
	}

	unsigned int numStates = atoi(argv[1]);
	uint64_t startTimer, stopTimer;

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
		SamplePacket **spArray =
			reinterpret_cast<SamplePacket **>(malloc(numStates * sizeof(void *)));
		for (unsigned int i = 0; i < numStates; i++) {
			spArray[i] = getPkt();
		}

		// Prepare one BufArray
		BufArray<SamplePacket> pktsIn(spArray, numStates);

		// Set data in input packets to 0
		for (unsigned int i = 0; i < numStates; i++) {
			*(reinterpret_cast<uint32_t *>(pktsIn[i]->getData())) = 0;
		}

		// Run the packet through the state machine
		startTimer = read_rdtsc();
		sm.runPktBatch(pktsIn);
		stopTimer = read_rdtsc();
		std::cout << "Insertion: " << stopTimer - startTimer << std::endl;

		// Run packets through SM
		startTimer = read_rdtsc();
		sm.runPktBatch(pktsIn);
		stopTimer = read_rdtsc();
		std::cout << "Run: " << stopTimer - startTimer << std::endl;

	} catch (exception *e) {
		// Just catch whatever fails there may be
		cout << endl << "FATAL:" << endl;
		cout << e->what() << endl;

		return 1;
	}

	return 0;
}
