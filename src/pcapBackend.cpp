#include "pcapBackend.hpp"
using namespace std;

PcapBackend::PcapBackend(
	string dev, array<uint8_t, 6> srcMac, StateMachine<TupleIdent, SamplePacket> &sm)
	: dev(dev), sm(sm), srcMac(srcMac) {
	handle = pcap_open_live(dev.c_str(), bufSize, 1, 1, errbuf);
	if (handle == NULL) {
		cout << "PcapBackend: pcap_open_live() failed" << endl;
		abort();
	}
};

PcapBackend::~PcapBackend() {
	pcap_close(handle);
	for (auto i : packetPool) {
		delete (i);
	}
};

void PcapBackend::sendBatch(vector<SamplePacket *> &pkts) {
	for (auto p : pkts) {
		// Set some valid ethernet header
		struct ether_header *eth = reinterpret_cast<ether_header *>(p->getData());
		memset(eth->ether_dhost, 6, 0xff);
		memcpy(eth->ether_shost, srcMac.data(), 6);
		eth->ether_type = htons(ETHERTYPE_IP);

		// Actually inject packet
		pcap_inject(handle, p->getData(), p->getDataLen());

		// Give buffer back to the pool
		packetPool.push_back(p);
	}
};

void PcapBackend::recvBatch(vector<SamplePacket *> &pkts) {
	for (auto p : pkts) {
		packetPool.push_back(p);
	}
	pkts.clear();

	struct pcap_pkthdr header;
	const u_char *pcapPacket;
	while ((pcapPacket = pcap_next(handle, &header))) {
		SamplePacket *pkt = getPkt();
		memcpy(pkt->getData(), pcapPacket, header.len);
		pkt->setDataLen(header.len);
		pkts.push_back(pkt);
	}
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
