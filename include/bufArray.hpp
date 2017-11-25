#ifndef BUFARRAY_HPP
#define BUFARRAY_HPP

#include <cassert>
#include <cstdint>
#include <utility>
#include <vector>

/*! Wrapper around MoonGen bufarrays
 *
 * BufArrays bundle an array of pointers to packets together with their count
 */
template <typename Packet> struct BufArray : public std::pair<Packet **, unsigned int> {

	BufArray(Packet **pkts, unsigned int numPkts)
		: std::pair<Packet **, unsigned int>(pkts, numPkts){};

	Packet **getArray() { return this->first; }
	unsigned int getNum() { return this->second; }
	void setNum(unsigned int num) { this->second = num; }

	void addPkt(Packet *pkt) {
		this->getArray()[this->getNum()] = pkt;
		this->second += 1;
	}

	Packet *operator[](unsigned int idx) { return this->getArray()[idx]; }

	Packet **begin() { return &this->getArray()[0]; }
	Packet **end() { return &this->getArray()[this->getNum()]; }
};

template <typename Packet> class BufArraySM {
private:
	Packet **pkts;
	std::vector<bool> sendMask;
	uint32_t numBufs;
	uint32_t numSlots;
	bool fromLua;

public:
	BufArraySM(Packet **pkts, uint32_t numPkts, bool fromLua = false) {
		this->pkts = pkts;
		assert((((uint64_t)this->pkts) & ((uint64_t)0x1)) == 0);
		if (fromLua) {
			this->fromLua = fromLua;
		}

		// In the beginning, the pkts** fits exactly
		numBufs = numPkts;
		numSlots = numPkts;

		sendMask.resize(numPkts);
		for (uint32_t i = 0; i < numPkts; i++) {
			sendMask[i] = true;
		}
	};

	~BufArraySM() {
		if (!fromLua) {
			free(pkts);
		}
	}

	void markDropPkt(uint32_t pktIdx) {
		assert(pktIdx < numBufs);
		sendMask[pktIdx] = false;
	};

	void markDropPkt(Packet *pkt) {
		assert(pkt != nullptr);
		for (uint32_t pktIdx = 0; pktIdx < numBufs; pktIdx++) {
			if (pkt == pkts[pktIdx]) {
				sendMask[pktIdx] = false;
			}
		}
	};

	void addPkt(Packet *pkt) {
		// Do we need to grow?
		if (numBufs == numSlots) {
			Packet **newPkts =
				reinterpret_cast<Packet **>(malloc(sizeof(Packet *) * numSlots * 2));
			for (uint32_t i = 0; i < numSlots; i++) {
				newPkts[i] = pkts[i];
			}

			// If the old memory is not from Lua, free it
			if (!fromLua) {
				free(pkts);
			}
			numSlots *= 2;

			sendMask.resize(numSlots);

			pkts = newPkts;

			// Now we allocated our own memory
			fromLua = false;
		}
		sendMask[numBufs] = true;
		pkts[numBufs++] = pkt;
	};

	uint32_t getSendCount() {
		uint32_t count = 0;
		for (auto i : sendMask) {
			if (i) {
				count++;
			}
		}

		return count;
	};

	uint32_t getFreeCount() {
		uint32_t sendCount = getSendCount();
		return numBufs - sendCount;
	}

	void getSendBufs(Packet **sendBufs) {
		uint32_t curSendBufs = 0;
		uint32_t curPkts = 0;
		uint32_t sendCount = getSendCount();

		while (curSendBufs < sendCount) {
			if (sendMask[curPkts]) {
				sendBufs[curSendBufs++] = pkts[curPkts++];
			} else {
				curPkts++;
			}
		}
	}

	void getFreeBufs(Packet **freeBufs) {
		uint32_t curFreeBufs = 0;
		uint32_t curPkts = 0;
		uint32_t freeCount = getFreeCount();

		while (curFreeBufs < freeCount) {
			if (!sendMask[curPkts]) {
				freeBufs[curFreeBufs++] = pkts[curPkts++];
			} else {
				curPkts++;
			}
		}
	}
};

#endif /* BUFARRAY_HPP */
