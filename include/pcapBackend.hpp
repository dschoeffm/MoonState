#ifdef WITH_PCAP

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

#include <net/ethernet.h>

#include "IPv4_5TupleL2Ident.hpp"
#include "bufArray.hpp"
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
	static constexpr unsigned int maxBufArraySize = 64;

	std::vector<SamplePacket *> packetPool;
	std::array<uint8_t, 6> srcMac;
	BufArray<SamplePacket> bufArray;

public:
	// WARNING: THIS CLASS IS NOT THREAD SAFE
	PcapBackend(std::string dev,
		std::array<uint8_t, 6> srcMac //,
									  // StateMachine<TupleIdent, SamplePacket> &sm
	);

	~PcapBackend();

	void sendBatch(BufArray<SamplePacket> &pkts);
	void freeBatch(BufArray<SamplePacket> &pkts);

	BufArray<SamplePacket> recvBatch();

	SamplePacket *getPkt();
};

#endif /* PCAPBACKEND_HPP */

#endif /* WITH_PCAP */
