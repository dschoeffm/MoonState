#ifdef WITH_PCAP

#include "pcapBackend.hpp"
#include "common.hpp"

#include <sys/types.h>
#include <unistd.h>

using namespace std;

PcapBackend::PcapBackend(
	string dev, array<uint8_t, 6> srcMac //,
										 // StateMachine<TupleIdent, SamplePacket> &sm
	)
	: dev(dev), /* sm(sm), */ srcMac(srcMac) {

	if (geteuid() != 0) {
		cout << "WARNING: Running pcap wihout root priviledges may be a bad idea" << endl;
	}

	handle = pcap_open_live(dev.c_str(), bufSize, 1, 1, errbuf);
	if (handle == NULL) {
		cout << "PcapBackend: pcap_open_live() failed" << endl;
		abort();
	}

	if (pcap_setnonblock(handle, 1, errbuf) < 0) {
		cout << "PcapBackend: pcap_setnonblock() failed" << endl;
		abort();
	}

	D(cout << "PcapBackend: device opened" << endl;)
};

PcapBackend::~PcapBackend() {
	pcap_close(handle);
	for (auto i : packetPool) {
		delete (i);
	}
};

void PcapBackend::sendBatch(BufArray<SamplePacket> &pkts) {
	auto sendCount = pkts.getSendCount();
	SamplePacket **spArray = reinterpret_cast<SamplePacket **>(malloc(sizeof(void *) * 64));
	pkts.getSendBufs(spArray);

	for (uint32_t i = 0; i < sendCount; i++) {
		SamplePacket *p = spArray[i];

		// Set some valid ethernet header
		struct ether_header *eth = reinterpret_cast<ether_header *>(p->getData());
		memset(eth->ether_dhost, 0xff, 6);
		memcpy(eth->ether_shost, srcMac.data(), 6);
		eth->ether_type = htons(ETHERTYPE_IP);

		// Actually inject packet
		if (pcap_inject(handle, p->getData(), p->getDataLen()) != (int)p->getDataLen()) {
			cout << "PcapBackend::sendBatch() pcap_inject() failed" << endl;
			abort();
		}

		// Give buffer back to the pool
		packetPool.push_back(p);
	}

	free(spArray);
};

void PcapBackend::freeBatch(BufArray<SamplePacket> &pkts) {
	auto freeCount = pkts.getFreeCount();
	SamplePacket **spArray = reinterpret_cast<SamplePacket **>(malloc(sizeof(void *) * 64));
	pkts.getFreeBufs(spArray);

	for (uint32_t i = 0; i < freeCount; i++) {
		packetPool.push_back(spArray[i]);
	}

	free(spArray);
};

BufArray<SamplePacket> *PcapBackend::recvBatch() {

	// Prepare the packet array
	SamplePacket **spArray = reinterpret_cast<SamplePacket **>(malloc(sizeof(void *) * 64));

	struct pcap_pkthdr header;
	const u_char *pcapPacket;

	unsigned int count = 0;

	while ((pcapPacket = pcap_next(handle, &header)) && count < 64) {
		// If a packet is larger than bufSize, I don't want it anyways...
		if (header.len > bufSize) {
			continue;
		}
		SamplePacket *pkt = getPkt();
		memcpy(pkt->getData(), pcapPacket, header.len);
		pkt->setDataLen(header.len);
		count++;
		D(cout << "PcapBackend::recvBatch() got another packet" << endl;)
	}

	// Prepare one BufArray
	BufArray<SamplePacket> *pkts = new BufArray<SamplePacket>(spArray, count);

	return pkts;
};

SamplePacket *PcapBackend::getPkt() {
	if (packetPool.empty()) {
		void *data = malloc(bufSize);
		return new SamplePacket(data, 0);
	} else {
		SamplePacket *sp = packetPool.back();
		packetPool.pop_back();
		sp->setDataLen(0);
		return sp;
	}
};

#endif /* WITH_PCAP */
