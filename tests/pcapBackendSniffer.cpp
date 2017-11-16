#ifdef WITH_PCAP

#include <iostream>
#include <string>
#include <vector>

#include <signal.h>

#include "pcapBackend.hpp"
#include "samplePacket.hpp"

using namespace std;

void printUsage(string str) {
	cout << "Usage: " << str << " <dev-name>" << endl;
	exit(0);
}

static bool interrupted = false;

void interrupt(int sig) {
	(void)sig;
	interrupted = true;
}

int main(int argc, char **argv) {

	signal(SIGINT, interrupt);

	if (argc < 2) {
		printUsage(string(argv[0]));
	}

	PcapBackend pcap(string(argv[1]), {{0, 0, 0, 0, 0, 0}});

	while (!interrupted) {
		BufArray<SamplePacket> pkts = pcap.recvBatch();
		for (auto p : pkts) {
			cout << endl << "Packet Hexdump:" << endl;
			hexdump(p->getData(), p->getDataLen());
		}
		pcap.freeBatch(pkts);
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
