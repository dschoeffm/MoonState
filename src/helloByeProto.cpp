#ifndef HELLOBYEPROTO_CPP
#define HELLOBYEPROTO_CPP

#include <cstdlib>
#include <sstream>

#include "IPv4_5TupleL2Ident.hpp"
#include "headers.hpp"
#include "helloByeProto.hpp"
#include "mbuf.hpp"
#include "samplePacket.hpp"

// using namespace Headers;
// using namespace std;

/*
 * ===================================
 * HelloByeServerHello
 * ===================================
 *
 */

template <class Identifier, class Packet>
HelloBye::HelloByeServerHello<Identifier, Packet>::HelloByeServerHello() {
	this->serverCookie = rand() % 10;
};

template <class Identifier, class Packet>
void HelloBye::HelloByeServerHello<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	char clientStr[] = "CLIENT HELLO:";
	if (memcmp(udp->getPayload(), clientStr, sizeof(clientStr) - 1) != 0) {
		std::cout << "HelloByeServerHello::fun() clientStr didn't match" << std::endl;
		state.transition(HelloByeServer::Terminate);
		funIface.freePkt();
		return;
	}

	// Get client cookie
	uint8_t cookieChar =
		reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(clientStr) - 1];
	this->clientCookie = static_cast<int>(cookieChar) - 48; // ASCII Conversion

	// Prepare new packet
	// Set payload string
	std::stringstream sstream;
	sstream << "SERVER HELLO:" << this->serverCookie << std::endl;
	std::string serverHelloStr = sstream.str();
	memcpy(udp->getPayload(), serverHelloStr.c_str(), serverHelloStr.length());

	// Set the IP header stuff
	// Leave the payload length alone for now...

	ipv4->ttl = 64;
	uint32_t tmp = ipv4->dstIP;
	ipv4->dstIP = ipv4->srcIP;
	ipv4->srcIP = tmp;
	ipv4->checksum = 0;

	// Set UDP checksum to 0 and hope for the best

	udp->checksum = 0;
	uint16_t tmp16 = udp->dstPort;
	udp->dstPort = udp->srcPort;
	udp->srcPort = tmp16;

	state.transition(HelloByeServer::Bye);
};

/*
 * ===================================
 * HelloByeServerBye
 * ===================================
 *
 */

template <class Identifier, class Packet>
HelloBye::HelloByeServerBye<Identifier, Packet>::HelloByeServerBye(
	const HelloByeServerHello<Identifier, Packet> *in)
	: clientCookie(in->clientCookie), serverCookie(in->serverCookie) {}

template <class Identifier, class Packet>
void HelloBye::HelloByeServerBye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Not needed in this function
	(void)funIface;

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	char clientStr[] = "CLIENT BYE:";
	if (memcmp(udp->getPayload(), clientStr, sizeof(clientStr) - 1) != 0) {
		std::cout << "HelloByeServerBye::fun() clientStr didn't match" << std::endl;
		state.transition(HelloByeServer::Terminate);
		return;
	}

	// Get client cookie
	uint8_t cookieChar =
		reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(clientStr) - 1];
	int recvCookie = static_cast<int>(cookieChar) - 48; // ASCII Conversion

	if (recvCookie != this->serverCookie) {
		std::cout << "HelloByeServerBye::fun() Client sent over wrong cookie" << std::endl;
		state.transition(HelloByeServer::Terminate);
		return;
	}

	// Prepare new packet
	// Set payload string
	std::stringstream sstream;
	sstream << "SERVER BYE:" << this->clientCookie << std::endl;
	std::string serverByeStr = sstream.str();
	memcpy(udp->getPayload(), serverByeStr.c_str(), serverByeStr.length());

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->ttl = 64;
	uint32_t tmp = ipv4->dstIP;
	ipv4->dstIP = ipv4->srcIP;
	ipv4->srcIP = tmp;
	ipv4->checksum = 0;

	// Set UDP checksum to 0 and hope for the best
	udp->checksum = 0;
	uint16_t tmp16 = udp->dstPort;
	udp->dstPort = udp->srcPort;
	udp->srcPort = tmp16;

	// We are done after this -> transition to Terminate
	state.transition(HelloByeServer::Terminate);
};

/*
 * ===================================
 * HelloByeClientHello
 * ===================================
 *
 */

template <class Identifier, class Packet>
HelloBye::HelloByeClientHello<Identifier, Packet>::HelloByeClientHello(
	uint32_t dstIp, uint16_t srcPort)
	: dstIp(dstIp), srcPort(srcPort) {
	this->clientCookie = rand() % 10;
};

template <class Identifier, class Packet>
void HelloBye::HelloByeClientHello<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	(void)funIface;

	memset(pkt->getData(), 0, pkt->getDataLen());

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());

	HelloByeClientConfig *config = HelloByeClientConfig::getInstance();

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->setVersion();
	ipv4->setIHL(5);
	ipv4->ttl = 64;
	ipv4->setPayloadLength(20);
	ipv4->setDstIP(this->dstIp);
	ipv4->setSrcIP(config->getSrcIP());
	ipv4->setProtoUDP();
	ipv4->checksum = 0;

	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	// Prepare new packet
	// Set payload string
	std::stringstream sstream;
	sstream << "CLIENT HELLO:" << this->clientCookie << std::endl;
	std::string clientHelloStr = sstream.str();
	memcpy(udp->getPayload(), clientHelloStr.c_str(), clientHelloStr.length());

	ether->ethertype = htons(0x0800);

	// Set UDP checksum to 0 and hope for the best
	udp->checksum = 0;
	udp->setDstPort(config->getDstPort());
	udp->setSrcPort(this->srcPort);
	udp->setPayloadLength(ipv4->getPayloadLength());

	state.transition(HelloByeClient::Bye);
};

/*
 * ===================================
 * HelloByeClientBye
 * ===================================
 *
 */

template <class Identifier, class Packet>
HelloBye::HelloByeClientBye<Identifier, Packet>::HelloByeClientBye(
	const HelloByeClientHello<Identifier, Packet> *in)
	: clientCookie(in->clientCookie) {}

template <class Identifier, class Packet>
void HelloBye::HelloByeClientBye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Not needed in this function
	(void)funIface;

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	char serverStr[] = "SERVER HELLO:";
	if (memcmp(udp->getPayload(), serverStr, sizeof(serverStr) - 1) != 0) {
		std::cout << "HelloByeClientBye::fun() serverStr didn't match" << std::endl;
		state.transition(HelloByeClient::Terminate);
		return;
	}

	// Get server cookie
	uint8_t cookieChar =
		reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(serverStr) - 1];
	this->serverCookie = static_cast<int>(cookieChar) - 48; // ASCII Conversion

	// Prepare new packet
	// Set payload string
	std::stringstream sstream;
	sstream << "CLIENT BYE:" << this->serverCookie << std::endl;
	std::string clientByeStr = sstream.str();

	// Zero out old data - just assume 20 bytes...
	memset(udp->getPayload(), 0, 20);
	memcpy(udp->getPayload(), clientByeStr.c_str(), clientByeStr.length());

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->ttl = 64;
	uint32_t tmp = ipv4->dstIP;
	ipv4->dstIP = ipv4->srcIP;
	ipv4->srcIP = tmp;
	ipv4->checksum = 0;

	// Set UDP checksum to 0 and hope for the best
	udp->checksum = 0;
	uint16_t tmp16 = udp->dstPort;
	udp->dstPort = udp->srcPort;
	udp->srcPort = tmp16;

	// We need to wait for the server reply
	state.transition(HelloByeClient::RecvBye);
};

/*
 * ===================================
 * HelloByeClientRecvBye
 * ===================================
 *
 */

template <class Identifier, class Packet>
HelloBye::HelloByeClientRecvBye<Identifier, Packet>::HelloByeClientRecvBye(
	const HelloByeClientBye<Identifier, Packet> *in)
	: clientCookie(in->clientCookie), serverCookie(in->serverCookie) {}

template <class Identifier, class Packet>
void HelloBye::HelloByeClientRecvBye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	char serverStr[] = "SERVER BYE:";
	if (memcmp(udp->getPayload(), serverStr, sizeof(serverStr) - 1) != 0) {
		std::cout << "HelloByeClientRecvBye::fun() serverStr didn't match" << std::endl;
		state.transition(HelloByeClient::Terminate);
		return;
	}

	// Get client cookie
	uint8_t cookieChar =
		reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(serverStr) - 1];
	int recvCookie = static_cast<int>(cookieChar) - 48; // ASCII Conversion

	if (recvCookie != this->clientCookie) {
		std::cout << "HelloByeClientRecvBye::fun() Server sent over wrong cookie"
				  << std::endl;
		state.transition(HelloByeClient::Terminate);
		return;
	}

	funIface.freePkt();

	// We are done after this -> transition to Terminate
	state.transition(HelloByeClient::Terminate);
};

	/*
	 * ===================================
	 * Prepare template instances
	 * ===================================
	 *
	 */

	/*
	 * This file is now included in the header
	 *
	template class HelloByeServerHello<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
	template class HelloByeServerBye<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
	template class HelloByeClientHello<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
	template class HelloByeClientBye<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
	template class HelloByeClientRecvBye<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;

	template class HelloByeServerHello<IPv4_5TupleL2Ident<mbuf>, mbuf>;
	template class HelloByeServerBye<IPv4_5TupleL2Ident<mbuf>, mbuf>;
	template class HelloByeClientHello<IPv4_5TupleL2Ident<mbuf>, mbuf>;
	template class HelloByeClientBye<IPv4_5TupleL2Ident<mbuf>, mbuf>;
	template class HelloByeClientRecvBye<IPv4_5TupleL2Ident<mbuf>, mbuf>;
	*/

#endif /* HELLOBYEPROTO_CPP */
