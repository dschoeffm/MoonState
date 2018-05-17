#include <cassert>
#include <iostream>
#include <limits>
#include <memory>

#include "IPv4_5TupleL2Ident.hpp"
#include "samplePacket.hpp"
#include <tbb/concurrent_hash_map.h>

/*
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
*/

using namespace std;

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

using ConnectionID = Identifier::ConnectionID;

struct State {
	uint64_t a[2];
};

class ConnectionPool {
private:
	struct TBBHasher {
		static std::size_t hash(const ConnectionID &x) {
			Identifier::Hasher h;
			return h(x);
		}

		static bool equal(const ConnectionID &x, const ConnectionID &y) { return x == y; }
	};

	tbb::concurrent_hash_map<ConnectionID, State, TBBHasher> newStates;

public:
	ConnectionPool() : newStates(10){};

	~ConnectionPool(){};

	/*! Add connection and state to the connection pool
	 *
	 * \param cID ID of the connection
	 * \param st State for the connection
	 */
	void add(ConnectionID &cID, State &st) { newStates.insert({cID, st}); };

	bool findAndErase(ConnectionID &cID) {
		typename tbb::concurrent_hash_map<ConnectionID, State, TBBHasher>::accessor it;
		if (newStates.find(it, cID)) {
			newStates.erase(it);
			return true;
		} else {
			return false;
		}
	};
};

// This just mallocs one SamplePacket
SamplePacket *getPkt() {
	uint32_t dataLen = 64;
	void *data = malloc(dataLen);
	SamplePacket *p = new SamplePacket(data, dataLen);
	return p;
}

uint64_t numMemAccess = 0;

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

	ConnectionID *cIDs = new ConnectionID[numStates];

	for (unsigned int i = 0; i < numStates; i++) {
		cIDs[i].val = i;
	}

	State s;
	ConnectionPool cp;

	startTimer = read_rdtsc();

	for (unsigned int i = 0; i < numStates; i++) {
		cp.add(cIDs[i], s);
	}

	stopTimer = read_rdtsc();
	std::cout << numStates << "," << stopTimer - startTimer << std::endl;

	return 0;
}
