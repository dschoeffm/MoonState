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

#include <arpa/inet.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <sys/socket.h>

using namespace std;

using Ident = IPv4_5TupleL2Ident<SamplePacket>;
using Packet = SamplePacket;

void printUsage(string str) {
	cout << "Usage: " << str << "<iface> <mac> <host> <port>" << endl;
	cout << "\tiface\tInterface to listen on" << endl;
	cout << "\tmac\tMac address of the interface" << endl;
	cout << "\thost\tServer address" << endl;
	cout << "\tport\tServer port" << endl;
	exit(0);
}

SamplePacket *getPkt() {
	uint32_t dataLen = 64;
	void *data = malloc(dataLen);
	SamplePacket *p = new SamplePacket(data, dataLen);
	return p;
}

int main(int argc, char **argv) {

	if (argc != 5) {
		printUsage(string(argv[0]));
	}

	struct ether_addr *addrStat = ether_aton(argv[2]);
	std::array<uint8_t, 6> addr;
	memcpy(addr.data(), addrStat->ether_addr_octet, 6);

	PcapBackend pcap(string(argv[1]), addr);

	struct in_addr ipAddr;
	inet_aton(argv[3], &ipAddr);

	// TODO build client config singleton

	// Uncomment this, as soon as the client config is used
	// uint16_t port = atoi(argv[4]);

	StateMachine<Ident, SamplePacket> sm;
	sm.registerEndStateID(HelloBye::HelloByeClient::Terminate);
	// sm.registerStartStateID(HelloByeClient::Hello, HelloByeClientHello<Ident,
	// Packet>::factory);

	sm.registerFunction(
		HelloBye::HelloByeClient::Hello, HelloBye::HelloByeClientHello<Ident, Packet>::run);
	sm.registerFunction(
		HelloBye::HelloByeClient::Bye, HelloBye::HelloByeClientBye<Ident, Packet>::run);

	sm.registerGetPktCB(getPkt);

	cout << "main(): Entering loop now" << endl;

	// TODO write later
	// sm.addState();

	while (1) {
		// vector<Packet*> vecIn, vecSend, vecFree;
		BufArray<SamplePacket> *pktsIn = pcap.recvBatch();

		sm.runPktBatch(*pktsIn);
		pcap.sendBatch(*pktsIn);
		pcap.freeBatch(*pktsIn);

		delete (pktsIn);
	}

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
