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
	this->serverCookie = rand()%10;
};

template <class Identifier, class Packet>
void HelloByeServerHello<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Get info from packet
	Ethernet *ether = reinterpret_cast<Ethernet *>(pkt->getData());
	IPv4 *ipv4 = reinterpret_cast<IPv4 *>(ether->getPayload());
	Udp *udp = reinterpret_cast<Udp *>(ipv4->getPayload());

	char clientStr[] = "CLIENT HELLO:";
	if (memcmp(udp->getPayload(), clientStr, sizeof(clientStr) - 1) != 0) {
		cout << "HelloByeServerHello::fun() clientStr didn't match" << endl;
		state.transition(HelloByeServer::Terminate);
		funIface.freePkt();
		return;
	}

	// Get client cookie
	uint8_t cookieChar = reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(clientStr)-1];
	this->clientCookie = static_cast<int>(cookieChar) - 48; // ASCII Conversion

	// Prepare new packet
	// Set payload string
	stringstream sstream;
	sstream << "SERVER HELLO:" << this->serverCookie << endl;
	string serverHelloStr = sstream.str();
	memcpy(udp->getPayload(), serverHelloStr.c_str(), serverHelloStr.length());

	// Set the IP header stuff
	// Leave the payload length alone for now...

	ipv4->ttl = 64;
	uint32_t tmp = ipv4->dstIP;
	ipv4->dstIP = ipv4->srcIP;
	ipv4->srcIP = tmp;
	ipv4->calcChecksum();


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
HelloByeServerBye<Identifier, Packet>::HelloByeServerBye(
	const HelloByeServerHello<Identifier, Packet> *in)
	: clientCookie(in->clientCookie), serverCookie(in->serverCookie) {}

template <class Identifier, class Packet>
void HelloByeServerBye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Not needed in this function
	(void) funIface;

	// Get info from packet
	Ethernet *ether = reinterpret_cast<Ethernet *>(pkt->getData());
	IPv4 *ipv4 = reinterpret_cast<IPv4 *>(ether->getPayload());
	Udp *udp = reinterpret_cast<Udp *>(ipv4->getPayload());

	char clientStr[] = "CLIENT BYE:";
	if (memcmp(udp->getPayload(), clientStr, sizeof(clientStr) - 1) != 0) {
		cout << "HelloByeServerBye::fun() clientStr didn't match" << endl;
		state.transition(HelloByeServer::Terminate);
		return;
	}

	// Get client cookie
	uint8_t cookieChar = reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(clientStr)-1];
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
	ipv4->ttl = 64;
	uint32_t tmp = ipv4->dstIP;
	ipv4->dstIP = ipv4->srcIP;
	ipv4->srcIP = tmp;
	ipv4->calcChecksum();

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
HelloByeClientHello<Identifier, Packet>::HelloByeClientHello(uint32_t dstIp, uint16_t srcPort)
	: dstIp(dstIp), srcPort(srcPort) {
	this->clientCookie = rand()%10;
};

template <class Identifier, class Packet>
void HelloByeClientHello<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// pkt is empty -> get one
	pkt = funIface.getPkt();
	memset(pkt->getData(), 0, pkt->getDataLen());

	// Get info from packet
	Ethernet *ether = reinterpret_cast<Ethernet *>(pkt->getData());
	IPv4 *ipv4 = reinterpret_cast<IPv4 *>(ether->getPayload());
	Udp *udp = reinterpret_cast<Udp *>(ipv4->getPayload());

	HelloByeClientConfig *config = HelloByeClientConfig::getInstance();

	// Prepare new packet
	// Set payload string
	stringstream sstream;
	sstream << "CLIENT HELLO:" << this->clientCookie << endl;
	string clientHelloStr = sstream.str();
	memcpy(udp->getPayload(), clientHelloStr.c_str(), clientHelloStr.length());

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->ttl = 64;
	ipv4->dstIP = this->dstIp;
	ipv4->srcIP = config->getSrcIP();
	ipv4->calcChecksum();

	// Set UDP checksum to 0 and hope for the best
	udp->checksum = 0;
	udp->dstPort = config->getDstPort();
	udp->srcPort = this->srcPort;

	state.transition(HelloByeClient::Bye);
};

/*
 * ===================================
 * HelloByeClientBye
 * ===================================
 *
 */

template <class Identifier, class Packet>
HelloByeClientBye<Identifier, Packet>::HelloByeClientBye(
	const HelloByeClientHello<Identifier, Packet> *in)
	: clientCookie(in->clientCookie) {}

template <class Identifier, class Packet>
void HelloByeClientBye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Not needed in this function
	(void) funIface;

	// Get info from packet
	Ethernet *ether = reinterpret_cast<Ethernet *>(pkt->getData());
	IPv4 *ipv4 = reinterpret_cast<IPv4 *>(ether->getPayload());
	Udp *udp = reinterpret_cast<Udp *>(ipv4->getPayload());

	char serverStr[] = "SERVER HELLO:";
	if (memcmp(udp->getPayload(), serverStr, sizeof(serverStr) - 1) != 0) {
		cout << "HelloByeClientBye::fun() serverStr didn't match" << endl;
		state.transition(HelloByeClient::Terminate);
		return;
	}

	// Get server cookie
	uint8_t cookieChar = reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(serverStr)-1];
	this->serverCookie = static_cast<int>(cookieChar) - 48; // ASCII Conversion

	// Prepare new packet
	// Set payload string
	stringstream sstream;
	sstream << "CLIENT BYE:" << this->serverCookie << endl;
	string clientByeStr = sstream.str();
	memcpy(udp->getPayload(), clientByeStr.c_str(), clientByeStr.length());

	// Set the IP header stuff
	// Leave the payload length alone for now...
	ipv4->ttl = 64;
	uint32_t tmp = ipv4->dstIP;
	ipv4->dstIP = ipv4->srcIP;
	ipv4->srcIP = tmp;
	ipv4->calcChecksum();

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
HelloByeClientRecvBye<Identifier, Packet>::HelloByeClientRecvBye(
	const HelloByeClientBye<Identifier, Packet> *in)
	: clientCookie(in->clientCookie), serverCookie(in->serverCookie) {}

template <class Identifier, class Packet>
void HelloByeClientRecvBye<Identifier, Packet>::fun(
	typename SM::State &state, Packet *pkt, typename SM::FunIface &funIface) {

	// Not needed in this function
	(void) funIface;

	// Get info from packet
	Ethernet *ether = reinterpret_cast<Ethernet *>(pkt->getData());
	IPv4 *ipv4 = reinterpret_cast<IPv4 *>(ether->getPayload());
	Udp *udp = reinterpret_cast<Udp *>(ipv4->getPayload());

	char serverStr[] = "SERVER BYE:";
	if (memcmp(udp->getPayload(), serverStr, sizeof(serverStr) - 1) != 0) {
		cout << "HelloByeClientRecvBye::fun() serverStr didn't match" << endl;
		state.transition(HelloByeClient::Terminate);
		return;
	}

	// Get client cookie
	uint8_t cookieChar = reinterpret_cast<uint8_t *>(udp->getPayload())[sizeof(serverStr)-1];
	int recvCookie = static_cast<int>(cookieChar) - 48; // ASCII Conversion

	if(recvCookie != this->clientCookie){
		cout << "HelloByeClientRecvBye::fun() Server sent over wrong cookie" << endl;
		state.transition(HelloByeClient::Terminate);
		return;
	}

	// We are done after this -> transition to Terminate
	state.transition(HelloByeClient::Terminate);
};

/*
 * ===================================
 * Prepare template instances
 * ===================================
 *
 */

template class HelloByeServerHello<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
template class HelloByeServerBye<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
template class HelloByeClientHello<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
template class HelloByeClientBye<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;
template class HelloByeClientRecvBye<IPv4_5TupleL2Ident<SamplePacket>, SamplePacket>;

/*
 * ===================================
 * Define Client config singleton
 * ===================================
 *
 */

HelloByeClientConfig* HelloByeClientConfig::instance = nullptr;

