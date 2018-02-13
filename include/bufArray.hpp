#ifndef BUFARRAY_HPP
#define BUFARRAY_HPP

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

#include "common.hpp"

/*! Wrapper around MoonGen bufarrays
 *
 * BufArrays bundle an array of pointers to packets together with their count
 */
/*
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
*/

/*! Wrapper around MoonGen bufarrays
 *
 * BufArrays bundle an array of pointers to packets together with their count.
 * It also tracks, which buffers should be send out or freed.
 * This array grows if needed.
 */
template <typename Packet> class BufArray {
private:
	Packet **pkts;
	std::vector<bool> sendMask;
	uint32_t numBufs;
	uint32_t numSlots;
	bool fromLua;

public:
	/*! Constructor
	 *
	 * \param pkts Pointer to pointers to buffers
	 * \param numPkts Number of buffers in pkts
	 * \param fromLua Set to true, if pkts is garbage-collected
	 */
	BufArray(Packet **pkts, uint32_t numPkts, bool fromLua = false) {
		this->pkts = pkts;
		this->fromLua = fromLua;

		if (numPkts == 0) {
			std::abort();
			throw std::runtime_error(
				"BufArray::BufArray() Please use an array with at least one slot");
		}

		// In the beginning, the pkts** fits exactly
		numBufs = numPkts;
		numSlots = numPkts;

		sendMask.resize(numPkts);
		for (uint32_t i = 0; i < numPkts; i++) {
			sendMask[i] = true;
		}
	};

	~BufArray() {
		if (!fromLua) {
			free(pkts);
		}
	}

	/*! Mark one packet as drop
	 *
	 * \param pktIdx Index of the packet to drop
	 */
	void markDropPkt(uint32_t pktIdx) {
		assert(pktIdx < numBufs);
		sendMask[pktIdx] = false;
	};

	/*! Mark one packet as drop
	 *
	 * \param pkt Pointer to the packet to drop
	 */
	void markDropPkt(Packet *pkt) {
		assert(pkt != nullptr);
		for (uint32_t pktIdx = 0; pktIdx < numBufs; pktIdx++) {
			if (pkt == pkts[pktIdx]) {
				sendMask[pktIdx] = false;
			}
		}
	};

	/*! Mark one packet as send
	 *
	 * \param pktIdx Index of the packet to send
	 */
	void markSendPkt(uint32_t pktIdx) {
		assert(pktIdx < numBufs);
		sendMask[pktIdx] = true;
	};

	/*! Add one packet to the BufArray
	 *
	 * This function may allocate new memory, to fit all packets into one array
	 *
	 * \param pkt Packe to add
	 */
	void addPkt(Packet *pkt) {
		DEBUG_ENABLED(std::cout << "BufArray::addPkt() Adding packet to array" << std::endl;)
		// Do we need to grow?
		if (numBufs == numSlots) {
			DEBUG_ENABLED(std::cout << "BufArray::addPkt() growing array" << std::endl;)

			// The +1 is for empty BufArrays
			Packet **newPkts =
				reinterpret_cast<Packet **>(malloc((sizeof(Packet *) * numSlots * 2)));
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
		} else {
			DEBUG_ENABLED(
				std::cout << "BufArray::addPkt() no need to grow the array" << std::endl;)
		}

		sendMask[numBufs] = true;
		pkts[numBufs++] = pkt;
	};

	/*! Get the number of packets currently marked as send
	 *
	 * \return Number of packets to be sent
	 */
	uint32_t getSendCount() const {
		uint32_t count = 0;
		for (auto i : sendMask) {
			if (i) {
				count++;
			}
		}

		return count;
	};

	/*! Get the number of packets currently marked as free
	 *
	 * \return Number of packets to be freed
	 */
	uint32_t getFreeCount() const {
		uint32_t sendCount = getSendCount();
		return numBufs - sendCount;
	}

	/*! Get all the packets which are to be sent
	 *
	 * \param sendBufs Array at least the size returned by getSendCount()
	 */
	void getSendBufs(Packet **sendBufs) const {
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

	/*! Get all the packets which are to be freed
	 *
	 * \param freeBufs Array at least the size returned by getFreeCount()
	 */
	void getFreeBufs(Packet **freeBufs) const {
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

	/*! Get the number of all packets in the BufArray
	 *
	 * \return Number of all packets
	 */
	uint32_t getTotalCount() const { return numBufs; }

	Packet *operator[](unsigned int idx) const { return pkts[idx]; }

	class iterator {
	private:
		friend BufArray;
		BufArray<Packet> *ba;
		uint32_t idx;
		iterator(BufArray<Packet> *ba, uint32_t idx) : ba(ba), idx(idx) {}

	public:
		iterator(const iterator &it) : ba(it.ba), idx(it.idx){};

		iterator operator++() { idx++; };

		bool operator==(const iterator &it) const {
			if (this->idx == it.idx) {
				return true;
			} else {
				return false;
			}
		}

		bool operator!=(const iterator &it) const { return !((*this) == it); }

		Packet *operator->() { return ba->pkts[idx]; }
		Packet *operator*() { return ba->pkts[idx]; }
	};

	iterator begin() { return iterator(this, 0); }
	iterator end() { return iterator(this, numBufs); }
};

#endif /* BUFARRAY_HPP */
