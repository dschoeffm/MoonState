#include <cstdlib>
#include <sstream>

#include "headers.hpp"
#include "helloByeProto.hpp"
#include "IPv4_5TupleL2Ident.hpp"
#include "samplePacket.hpp"

using namespace Headers;
using namespace std;

/*
 * ===================================
 * HelloByeServerHello
 * ===================================
 *
 */

template <class Identifier, class Packet>
HelloByeServerHello<Identifier, Packet>::HelloByeServerHello() {
	this->serverCookie = rand();
};

template <class Identifier, class Packet>
void HelloByeServerHello<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Get info from packet
	Ethernet *ether = reinterpret_cast<Ethernet *>(pkt->getData());
	IPv4 *ipv4 = reinterpret_cast<IPv4 *>(ether->getPayload());
	Udp *udp = reinterpret_cast<Udp *>(ipv4->getPayload());

	char clientStr[] = "CLIENT HELLO:";
	if (memcmp(udp->getPayload(), clientStr, sizeof(clientStr) - 1) == 0) {
		cout << "HelloByeServerHello::fun() clientStr didn't match" << endl;
		state.transition(HelloByeServer::Terminate);
		return;
	}

	// Get client cookie
	uint8_t cookieChar = reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(clientStr)];
	this->clientCookie = static_cast<int>(cookieChar) - 48; // ASCII Conversion

	// Prepare new packet
	// Set payload string
	stringstream sstream;
	sstream << "SERVER HELLO:" << this->serverCookie << endl;
	string serverHelloStr = sstream.str();
	memcpy(udp->getPayload(), serverHelloStr.c_str(), serverHelloStr.length());

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->ip_ttl = 64;
	uint32_t tmp = ipv4->ip_dst.s_addr;
	ipv4->ip_dst.s_addr = ipv4->ip_src.s_addr;
	ipv4->ip_src.s_addr = tmp;
	ipv4->calcChecksum();

	// Set UDP checksum to 0 and hope for the best
	udp->check = 0;

	funIface.addPktToSend(pkt);
};

/*
 * ===================================
 * HelloByeServerBye
 * ===================================
 *
 */

template <class Identifier, class Packet>
HelloByeServerBye<Identifier, Packet>::HelloByeServerBye(
	const HelloByeServerHello<Identifier, Packet> *in)
	: clientCookie(in->clientCookie), serverCookie(in->serverCookie) {}

template <class Identifier, class Packet>
void HelloByeServerBye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Get info from packet
	Ethernet *ether = reinterpret_cast<Ethernet *>(pkt->getData());
	IPv4 *ipv4 = reinterpret_cast<IPv4 *>(ether->getPayload());
	Udp *udp = reinterpret_cast<Udp *>(ipv4->getPayload());

	char clientStr[] = "CLIENT BYE:";
	if (memcmp(udp->getPayload(), clientStr, sizeof(clientStr) - 1) == 0) {
		cout << "HelloByeServerBye::fun() clientStr didn't match" << endl;
		state.transition(HelloByeServer::Terminate);
		return;
	}

	// Get client cookie
	uint8_t cookieChar = reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(clientStr)];
	int recvCookie = static_cast<int>(cookieChar) - 48; // ASCII Conversion

	if(recvCookie != this->serverCookie){
		cout << "HelloByeServerBye::fun() Client sent over wrong cookie" << endl;
		state.transition(HelloByeServer::Terminate);
		return;
	}

	// Prepare new packet
	// Set payload string
	stringstream sstream;
	sstream << "SERVER BYE:" << this->clientCookie << endl;
	string serverByeStr = sstream.str();
	memcpy(udp->getPayload(), serverByeStr.c_str(), serverByeStr.length());

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->ip_ttl = 64;
	uint32_t tmp = ipv4->ip_dst.s_addr;
	ipv4->ip_dst.s_addr = ipv4->ip_src.s_addr;
	ipv4->ip_src.s_addr = tmp;
	ipv4->calcChecksum();

	// Set UDP checksum to 0 and hope for the best
	udp->check = 0;

	funIface.addPktToSend(pkt);
};

/*
 * ===================================
 * HelloByeClientHello
 * ===================================
 *
 */

template <class Identifier, class Packet>
HelloByeClientHello<Identifier, Packet>::HelloByeClientHello() {
	this->clientCookie = rand();
};

template <class Identifier, class Packet>
void HelloByeClientHello<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Get info from packet
	Ethernet *ether = reinterpret_cast<Ethernet *>(pkt->getData());
	IPv4 *ipv4 = reinterpret_cast<IPv4 *>(ether->getPayload());
	Udp *udp = reinterpret_cast<Udp *>(ipv4->getPayload());

	// Prepare new packet
	// Set payload string
	stringstream sstream;
	sstream << "SERVER HELLO:" << this->clientCookie << endl;
	string serverHelloStr = sstream.str();
	memcpy(udp->getPayload(), serverHelloStr.c_str(), serverHelloStr.length());

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->ip_ttl = 64;
	uint32_t tmp = ipv4->ip_dst.s_addr;
	ipv4->ip_dst.s_addr = ipv4->ip_src.s_addr;
	ipv4->ip_src.s_addr = tmp;
	ipv4->calcChecksum();

	// Set UDP checksum to 0 and hope for the best
	udp->check = 0;

	state.transition(HelloByeClient::Bye);

	funIface.addPktToSend(pkt);
};

template class HelloByeServerHello<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
template class HelloByeServerBye<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
template class HelloByeClientHello<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
template class HelloByeClientBye<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;

