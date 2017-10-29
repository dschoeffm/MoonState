#ifndef PCAPBACKEND_HPP
#define PCAPBACKEND_HPP

#include <pcap.h>

#include <array>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include <net/ethernet.h>

#include "IPv4_5TupleL2Ident.hpp"
#include "samplePacket.hpp"
#include "stateMachine.hpp"

class PcapBackend {
private:
	using TupleIdent = IPv4_5TupleL2Ident<SamplePacket>;

	pcap_t *handle;
	std::string dev;
	char errbuf[PCAP_ERRBUF_SIZE];
	//	StateMachine<TupleIdent, SamplePacket> &sm;

	static constexpr uint32_t bufSize = 2048;

	std::vector<SamplePacket *> packetPool;
	std::array<uint8_t, 6> srcMac;

public:
	PcapBackend(std::string dev, std::array<uint8_t, 6> srcMac //,
		// StateMachine<TupleIdent, SamplePacket> &sm
		);

	~PcapBackend();

	void sendBatch(std::vector<SamplePacket *> &pkts);
	void freeBatch(std::vector<SamplePacket *> &pkts);
	void recvBatch(std::vector<SamplePacket *> &pkts);

	SamplePacket *getPkt();
};

#endif /* PCAPBACKEND_HPP */
