
#include <iostream>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

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

	cout << endl;

	const size_t bufSize = 100;

	PcapBackend pcap(string(argv[1]), {{0x4, 0x5, 0x6, 0x7, 0x8, 0x9}});

	SamplePacket *sp = new SamplePacket(malloc(bufSize), bufSize);
	BufArray<SamplePacket> bufArray(
		reinterpret_cast<SamplePacket **>(malloc(sizeof(void *))), 0);
	bufArray.addPkt(sp);

	memset(sp->getData(), 0, bufSize);

	struct ip *ip = reinterpret_cast<struct ip *>(((uint8_t *)sp->getData()) + 14);
	ip->ip_hl = 5;
	ip->ip_v = 4;
	ip->ip_tos = 0;
	ip->ip_len = htons(sizeof(struct ip) + sizeof(struct icmp));
	ip->ip_id = 0;
	ip->ip_off = 0;
	ip->ip_ttl = 64;
	ip->ip_p = IPPROTO_ICMP;
	ip->ip_sum = 0;
	if (inet_aton("192.168.0.120", &ip->ip_src) == 0) {
		cout << "inet_aton() failed" << endl;
	}
	if (inet_aton("8.8.8.8", &ip->ip_dst) == 0) {
		cout << "inet_aton() failed" << endl;
	}

	struct icmp *icmp = reinterpret_cast<struct icmp *>(((uint8_t *)ip) + sizeof(struct ip));

	icmp->icmp_type = ICMP_ECHO;
	icmp->icmp_code = 0;
	icmp->icmp_cksum = 0;
	icmp->icmp_hun.ih_idseq.icd_id = 0xbeef;
	icmp->icmp_hun.ih_idseq.icd_seq = 0;

	cout << "Assembled packet:" << endl;
	hexdump(bufArray[0]->getData(), bufArray[0]->getDataLen());

	cout << "main() sending packets" << endl;

	pcap.sendBatch(bufArray);

	cout << "reached end of main()" << endl << endl;

	return 0;
}
