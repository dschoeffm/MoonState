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

using namespace std;

using Ident = IPv4_5TupleL2Ident<SamplePacket>;
using Packet = SamplePacket;

void printUsage(string str) {
	cout << "Usage: " << str << "<iface> <mac>" << endl;
	cout << "\tiface\tInterface to listen on" << endl;
	cout << "\tmac\tMac address of the interface" << endl;
	cout << "\tServer runs on port 1337" << endl;
	exit(0);
}

SamplePacket *getPkt() {
	uint32_t dataLen = 64;
	void *data = malloc(dataLen);
	SamplePacket *p = new SamplePacket(data, dataLen);
	return p;
}

int main(int argc, char **argv) {

	if (argc != 3) {
		printUsage(string(argv[0]));
	}

	struct ether_addr *addrStat = ether_aton(argv[2]);
	std::array<uint8_t, 6> addr;
	memcpy(addr.data(), addrStat->ether_addr_octet, 6);

	PcapBackend pcap(string(argv[1]), addr);

	StateMachine<Ident, SamplePacket> sm;
	sm.registerEndStateID(HelloBye::HelloByeServer::Terminate);
	sm.registerStartStateID(HelloBye::HelloByeServer::Hello,
		HelloBye::HelloByeServerHello<Ident, Packet>::factory);

	sm.registerFunction(
		HelloBye::HelloByeServer::Hello, HelloBye::HelloByeServerHello<Ident, Packet>::run);
	sm.registerFunction(
		HelloBye::HelloByeServer::Bye, HelloBye::HelloByeServerBye<Ident, Packet>::run);

	sm.registerGetPktCB(getPkt);

	cout << "main(): Entering loop now" << endl;

	while (1) {
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
