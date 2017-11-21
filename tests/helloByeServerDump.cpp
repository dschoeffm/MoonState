#ifdef WITH_PCAP

#include <array>
#include <iostream>
#include <string>
#include <vector>

#include "IPv4_5TupleL2Ident.hpp"
#include "helloByeProto.hpp"
#include "pcapBackend.hpp"
#include "samplePacket.hpp"
#include "stateMachine.hpp"

#include <netinet/ether.h>

#include <pcap/pcap.h>

using namespace std;

using Ident = IPv4_5TupleL2Ident<SamplePacket>;
using Packet = SamplePacket;

void printUsage(string str) {
	cout << "Usage: " << str << " <dumpfile>" << endl;
	exit(0);
}

SamplePacket *getPkt() {
	uint32_t dataLen = 64;
	void *data = malloc(dataLen);
	SamplePacket *p = new SamplePacket(data, dataLen);
	return p;
}

int main(int argc, char **argv) {

	if (argc != 2) {
		printUsage(string(argv[0]));
	}

	char errbuf[PCAP_ERRBUF_SIZE];

	pcap_t *handler = pcap_open_offline(argv[1], errbuf);

	if (handler == nullptr) {
		cout << "pcap_open_offline() failed" << endl;
		cout << "Reason: " << errbuf << endl;
	}

	StateMachine<Ident, SamplePacket> sm;
	sm.registerEndStateID(HelloBye::HelloByeServer::Terminate);
	sm.registerStartStateID(HelloBye::HelloByeServer::Hello,
		HelloBye::HelloByeServerHello<Ident, Packet>::factory);

	sm.registerFunction(
		HelloBye::HelloByeServer::Hello, HelloBye::HelloByeServerHello<Ident, Packet>::run);
	sm.registerFunction(
		HelloBye::HelloByeServer::Bye, HelloBye::HelloByeServerBye<Ident, Packet>::run);

	sm.registerGetPktCB(getPkt);

	pcap_pkthdr clientPacket1Hdr;
	uint8_t *clientPacket1;
	const uint8_t *pcapC1P_p = pcap_next(handler, &clientPacket1Hdr);
	clientPacket1 = reinterpret_cast<uint8_t *>(malloc(clientPacket1Hdr.len));
	memcpy(clientPacket1, pcapC1P_p, clientPacket1Hdr.len);
	SamplePacket spc1(clientPacket1, clientPacket1Hdr.len);

	pcap_pkthdr serverPacket1Hdr;
	uint8_t *serverPacket1;
	const uint8_t *pcapS1P_p = pcap_next(handler, &serverPacket1Hdr);
	serverPacket1 = reinterpret_cast<uint8_t *>(malloc(serverPacket1Hdr.len));
	memcpy(serverPacket1, pcapS1P_p, serverPacket1Hdr.len);
	SamplePacket sps1(serverPacket1, serverPacket1Hdr.len);

	pcap_pkthdr clientPacket2Hdr;
	uint8_t *clientPacket2;
	const uint8_t *pcapC2P_p = pcap_next(handler, &clientPacket2Hdr);
	clientPacket2 = reinterpret_cast<uint8_t *>(malloc(clientPacket2Hdr.len));
	memcpy(clientPacket2, pcapC2P_p, clientPacket2Hdr.len);
	SamplePacket spc2(clientPacket2, clientPacket2Hdr.len);

	pcap_pkthdr serverPacket2Hdr;
	uint8_t *serverPacket2;
	const uint8_t *pcapS2P_p = pcap_next(handler, &serverPacket2Hdr);
	serverPacket2 = reinterpret_cast<uint8_t *>(malloc(serverPacket2Hdr.len));
	memcpy(serverPacket2, pcapS2P_p, serverPacket2Hdr.len);
	SamplePacket sps2(serverPacket2, serverPacket2Hdr.len);

	uint8_t clientCookie = 0;
	uint8_t serverCookie = 0;

	cout << "main(): Processing packets now" << endl;

	{
		BufArray<SamplePacket> pktsIn(
			reinterpret_cast<SamplePacket **>(malloc(sizeof(void *))), 0);
		BufArray<SamplePacket> pktsSend(
			reinterpret_cast<SamplePacket **>(malloc(sizeof(void *) * pktsIn.getNum())), 0);
		BufArray<SamplePacket> pktsFree(
			reinterpret_cast<SamplePacket **>(malloc(sizeof(void *) * pktsIn.getNum())), 0);

		pktsIn.addPkt(&spc1);
		cout << "Dump of packet input" << endl;
		hexdump(spc1.getData(), spc1.getDataLen());

		clientCookie = reinterpret_cast<uint8_t *>(spc1.getData())[0x37];
		cout << "clientCookie: 0x" << hex << static_cast<int>(clientCookie) << endl;

		sm.runPktBatch(pktsIn, pktsSend, pktsFree);
		assert(pktsSend.getNum() == 1);
		assert(pktsFree.getNum() == 0);
		cout << "Dump of packet output" << endl;
		hexdump(spc1.getData(), spc1.getDataLen());

		serverCookie = reinterpret_cast<uint8_t *>(spc1.getData())[0x37];
		cout << "serverCookie: 0x" << hex << static_cast<int>(serverCookie) << endl;
	}
	{
		reinterpret_cast<uint8_t *>(spc2.getData())[0x35] = serverCookie;

		BufArray<SamplePacket> pktsIn(
			reinterpret_cast<SamplePacket **>(malloc(sizeof(void *))), 0);
		BufArray<SamplePacket> pktsSend(
			reinterpret_cast<SamplePacket **>(malloc(sizeof(void *) * pktsIn.getNum())), 0);
		BufArray<SamplePacket> pktsFree(
			reinterpret_cast<SamplePacket **>(malloc(sizeof(void *) * pktsIn.getNum())), 0);

		pktsIn.addPkt(&spc2);
		cout << "Dump of packet input" << endl;
		hexdump(spc2.getData(), spc2.getDataLen());

		sm.runPktBatch(pktsIn, pktsSend, pktsFree);
		assert(pktsSend.getNum() == 1);
		assert(pktsFree.getNum() == 0);

		assert(reinterpret_cast<uint8_t *>(spc2.getData())[0x35] == clientCookie);

		cout << "Dump of packet output" << endl;
		hexdump(spc2.getData(), spc2.getDataLen());
	}

	pcap_close(handler);

	cout << endl << "-----------------------------------------" << endl;
	cout << "All tests ran through without aborting :)" << endl;
	cout << "-----------------------------------------" << endl << endl;

	return 0;
}

#else

#include <iostream>

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	std::cout << "Built without pcap..." << std::endl;
	return 0;
}

#endif
