#include "pcapBackend.hpp"
#include "common.hpp"

#include <sys/types.h>
#include <unistd.h>

using namespace std;

PcapBackend::PcapBackend(string dev, array<uint8_t, 6> srcMac //,
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
		memset(eth->ether_dhost, 0xff, 6);
		memcpy(eth->ether_shost, srcMac.data(), 6);
		eth->ether_type = htons(ETHERTYPE_IP);

		// Actually inject packet
		if(pcap_inject(handle, p->getData(), p->getDataLen()) != (int) p->getDataLen()){
			cout << "PcapBackend::sendBatch() pcap_inject() failed" << endl;
			abort();
		}

		// Give buffer back to the pool
		packetPool.push_back(p);
	}
};

void PcapBackend::freeBatch(vector<SamplePacket *> &pkts) {
	for (auto p : pkts) {
		packetPool.push_back(p);
	}
	pkts.clear();
};

void PcapBackend::recvBatch(vector<SamplePacket *> &pkts) {
	for (auto p : pkts) {
		packetPool.push_back(p);
	}
	pkts.clear();

	struct pcap_pkthdr header;
	const u_char *pcapPacket;
	while ((pcapPacket = pcap_next(handle, &header))) {
		// If a packet is larger than bufSize, I don't want it anyways...
		if (header.len > bufSize) {
			continue;
		}
		SamplePacket *pkt = getPkt();
		memcpy(pkt->getData(), pcapPacket, header.len);
		pkt->setDataLen(header.len);
		pkts.push_back(pkt);
		D(cout << "PcapBackend::recvBatch() got another packet" << endl;)
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
