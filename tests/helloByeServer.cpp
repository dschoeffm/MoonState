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
	sm.registerEndStateID(HelloByeServer::Terminate);

	sm.registerFunction(HelloByeServer::Hello, HelloByeServerHello<Ident, Packet>::run);
	sm.registerFunction(HelloByeServer::Bye, HelloByeServerBye<Ident, Packet>::run);

	sm.registerGetPktCB(getPkt);

	while(1){
		vector<Packet*> vecIn, vecSend, vecFree;
		pcap.recvBatch(vecIn);
		sm.runPktBatch(vecIn, vecSend, vecFree);
		pcap.sendBatch(vecSend);
		pcap.freeBatch(vecFree);
	}

	return 0;
}
