#ifndef BUFARRAY_HPP
#define BUFARRAY_HPP

#include <cstdint>
#include <iterator>
#include <utility>

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

	Packet* operator[](unsigned int idx){
		return this->getArray()[idx];
	}

	//	typedef Packet *iterator;
	//	typedef const Packet *const_iterator;
	Packet **begin() { return &this->getArray()[0]; }
	Packet **end() { return &this->getArray()[this->getNum()]; }
};

#endif /* BUFARRAY_HPP */
