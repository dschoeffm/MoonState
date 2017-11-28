#ifndef HELLOBYEPROTO_CPP
#define HELLOBYEPROTO_CPP

#include <cstdlib>
#include <sstream>

#include "IPv4_5TupleL2Ident.hpp"
#include "headers.hpp"
#include "helloBye2Proto.hpp"
#include "mbuf.hpp"
#include "samplePacket.hpp"

// using namespace Headers;
// using namespace std;

namespace HelloBye2 {

namespace Server {

/*
 * ===================================
 * HelloByeServerHello
 * ===================================
 *
 */

template <class Identifier, class Packet> Hello<Identifier, Packet>::Hello() {
	this->serverCookie = rand();
};

template <class Identifier, class Packet>
void Hello<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	(void)state;

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	if ((msg->role != msg::ROLE_CLIENT) || (msg->msg != msg::MSG_HELLO)) {
		std::cout << "HelloBye2::Server::Hello::fun() client hello wrong" << std::endl;
		funIface.transition(States::Terminate);
		funIface.freePkt();
		return;
	}

	// Get client cookie
	this->clientCookie = msg->cookie;

	msg->role = msg::ROLE_SERVER;
	msg->msg = msg::MSG_HELLO;
	msg->cookie = serverCookie;

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

	funIface.transition(States::Bye);
};

/*
 * ===================================
 * HelloByeServerBye
 * ===================================
 *
 */

template <class Identifier, class Packet>
Bye<Identifier, Packet>::Bye(const Hello<Identifier, Packet> *in)
	: clientCookie(in->clientCookie), serverCookie(in->serverCookie) {}

template <class Identifier, class Packet>
void Bye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	(void)state;

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	if ((msg->role != msg::ROLE_CLIENT) || (msg->msg != msg::MSG_BYE)) {
		std::cout << "HelloBye2::Server::Bye::fun() msg fields wrong" << std::endl;
		funIface.transition(States::Terminate);
		return;
	}

	if (msg->cookie != this->serverCookie) {
		std::cout << "HelloBye2::Server::Bye::fun() Client sent over wrong cookie"
				  << std::endl;
		funIface.transition(States::Terminate);
		return;
	}

	// Prepare new packet
	msg->cookie = this->clientCookie;
	msg->role = msg::ROLE_SERVER;
	msg->msg = msg::MSG_BYE;

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
	funIface.transition(States::Terminate);
};
}; // namespace Server

namespace Client {
/*
 * ===================================
 * HelloByeClientHello
 * ===================================
 *
 */

template <class Identifier, class Packet>
Hello<Identifier, Packet>::Hello(uint32_t dstIp, uint16_t srcPort, uint64_t ident)
	: dstIp(dstIp), srcPort(srcPort), ident(ident) {
	this->clientCookie = rand();
};

template <class Identifier, class Packet>
void Hello<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	(void)state;

	memset(pkt->getData(), 0, pkt->getDataLen());

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());

	HelloBye2ClientConfig *config = HelloBye2ClientConfig::getInstance();

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->setVersion();
	ipv4->setIHL(5);
	ipv4->ttl = 64;
	ipv4->setLength(100 - 14);
	ipv4->setDstIP(this->dstIp);
	ipv4->setSrcIP(config->getSrcIP());
	ipv4->setProtoUDP();
	ipv4->checksum = 0;

	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());
	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	// Prepare msg
	msg->ident = ident;
	msg->role = msg::ROLE_CLIENT;
	msg->cookie = clientCookie;
	msg->msg = msg::MSG_HELLO;

	ether->ethertype = htons(0x0800);

	// Set UDP checksum to 0 and hope for the best
	udp->checksum = 0;
	udp->setDstPort(config->getDstPort());
	udp->setSrcPort(this->srcPort);
	udp->setPayloadLength(50);

	funIface.transition(States::Bye);
};

/*
 * ===================================
 * HelloByeClientBye
 * ===================================
 *
 */

template <class Identifier, class Packet>
Bye<Identifier, Packet>::Bye(const Hello<Identifier, Packet> *in)
	: clientCookie(in->clientCookie) {}

template <class Identifier, class Packet>
void Bye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	(void)state;

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	if ((msg->role != msg::ROLE_SERVER) || (msg->msg != msg::MSG_HELLO)) {
		std::cout << "HelloBye2::Client::Bye::fun() msg fields wrong" << std::endl;
		funIface.transition(States::Terminate);
		return;
	}

	// Get server cookie
	this->serverCookie = msg->cookie;

	msg->cookie = serverCookie;
	msg->role = msg::ROLE_CLIENT;
	msg->msg = msg::MSG_BYE;

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
	funIface.transition(States::RecvBye);
};

/*
 * ===================================
 * HelloByeClientRecvBye
 * ===================================
 *
 */

template <class Identifier, class Packet>
RecvBye<Identifier, Packet>::RecvBye(const Bye<Identifier, Packet> *in)
	: clientCookie(in->clientCookie), serverCookie(in->serverCookie) {}

template <class Identifier, class Packet>
void RecvBye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	(void)state;

	// Get info from packet
	Headers::Ethernet *ether = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
	Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(ether->getPayload());
	Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

	struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());

	if ((msg->role != msg::ROLE_SERVER) || (msg->msg != msg::MSG_BYE)) {
		std::cout << "HelloBye2::Client::RecvBye::fun() msg fields wrong" << std::endl;
		funIface.transition(States::Terminate);
		return;
	}

	if (msg->cookie != this->clientCookie) {
		std::cout << "HelloByeClientRecvBye::fun() Server sent over wrong cookie"
				  << std::endl;
		funIface.transition(States::Terminate);
		return;
	}

	funIface.freePkt();

	// We are done after this -> transition to Terminate
	funIface.transition(States::Terminate);
};
}; // namespace Client
}; // namespace HelloBye2

#endif /* HELLOBYEPROTO_CPP */
