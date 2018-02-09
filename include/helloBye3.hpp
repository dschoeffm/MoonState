#ifndef HELLOBYE3PROTO_HPP
#define HELLOBYE3PROTO_HPP

#include <mutex>
#include <sstream>

#include "common.hpp"
#include "exceptions.hpp"
#include "headers.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

namespace HelloBye3 {

struct __attribute__((packed)) msg {
	uint64_t ident;
	uint8_t role;
	uint8_t msg;
	uint8_t cookie;

	static constexpr uint8_t ROLE_CLIENT = 0;
	static constexpr uint8_t ROLE_SERVER = 1;

	static constexpr uint8_t MSG_HELLO = 0;
	static constexpr uint8_t MSG_BYE = 1;
};

/*
 * ===================================
 * Identifier
 * ===================================
 *
 */

// This is the same as in HelloBye2
template <typename Packet> class Identifier {
public:
	class ConnectionID {
	public:
		uint64_t ident;

		bool operator==(const ConnectionID &c) const { return ident == c.ident; };
		bool operator<(const ConnectionID &c) const { return ident < c.ident; };
		operator std::string() const {
			std::stringstream sstream;
			sstream << ident;
			return sstream.str();
		};
		ConnectionID(const ConnectionID &c) : ident(c.ident){};
		ConnectionID() : ident(0){};
		ConnectionID(uint64_t i) : ident(i){};
	};
	struct Hasher {
		uint64_t operator()(const ConnectionID &c) const { return c.ident; };
	};
	static ConnectionID identify(Packet *pkt) {
		Headers::Ethernet *eth = reinterpret_cast<Headers::Ethernet *>(pkt->getData());
		if (eth->getEthertype() != Headers::Ethernet::ETHERTYPE_IPv4) {
			throw new PacketNotIdentified();
		}

		Headers::IPv4 *ipv4 = reinterpret_cast<Headers::IPv4 *>(eth->getPayload());
		if (ipv4->proto != Headers::IPv4::PROTO_UDP) {
			throw new PacketNotIdentified();
		}

		Headers::Udp *udp = reinterpret_cast<Headers::Udp *>(ipv4->getPayload());

		struct msg *msg = reinterpret_cast<struct msg *>(udp->getPayload());
		return msg->ident;
	};

	static ConnectionID getDelKey() {
		ConnectionID id;
		id.ident = std::numeric_limits<uint64_t>::max();
		return id;
	};

	static ConnectionID getEmptyKey() {
		ConnectionID id;
		id.ident = std::numeric_limits<uint64_t>::max() - 1;
		return id;
	};
};

/*
 * ===================================
 * Forward declarations
 * ===================================
 *
 */

/*
 * ===================================
 * Server
 * ===================================
 *
 */

namespace Server {
struct server {
	int clientCookie;
	int serverCookie;
};
struct States {
	static constexpr StateID Hello = 0;
	static constexpr StateID Bye = 1;
	static constexpr StateID Terminate = 2;
};

void *factory(Identifier<mbuf>::ConnectionID id);

void runHello(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface);

void runBye(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface);

}; // namespace Server

/*
 * ===================================
 * Client
 * ===================================
 *
 */

namespace Client {
struct client {
	uint64_t ident;
	uint32_t srcIp;
	uint32_t dstIp;
	int clientCookie;
	int serverCookie;
	uint16_t srcPort;
	uint16_t dstPort;
};

struct States {
	static constexpr StateID Hello = 0;
	static constexpr StateID Bye = 1;
	static constexpr StateID RecvBye = 2;
	static constexpr StateID Terminate = 3;
};

StateMachine<Identifier<mbuf>, mbuf>::State createHello(
	uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort, uint64_t ident);

void runHello(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface);

void runBye(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface);

void runRecvBye(StateMachine<Identifier<mbuf>, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier<mbuf>, mbuf>::FunIface &funIface);

} // namespace Client

}; // namespace HelloBye3

/*
 * From here on down is the C interface to the above C++ functions
 */

extern "C" {

/*
 * XXX -------------------------------------------- XXX
 *       Server
 * XXX -------------------------------------------- XXX
 */

/*! Init a HelloBye server
 *
 *	\return void* to the object (opaque)
 */
void *HelloBye3_Server_init();

/*! Process a batch of packets
 *
 * \param obj object returned by HelloByeServer_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 */
void *HelloBye3_Server_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Get the packets to send and free
 *
 * \param obj object returned by HelloByeServer_process
 * \param sendPkts Packets to send out (size if given in sendCount)
 * \param freePkts Packets to free (size if given in freeCount)
 */
void HelloBye3_Server_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Free recources used by the state machine
 *
 * \param obj object returned by HelloByeServer_init()
 */
void HelloBye3_Server_free(void *obj);

/*
 * XXX -------------------------------------------- XXX
 *       Client
 * XXX -------------------------------------------- XXX
 */

/*! Init a HelloBye client
 *
 * \return void* to the object (opaque)
 */
void *HelloBye3_Client_init();

/*! Open a connection to a server
 *
 * \param obj object returned by HelloBye3_Client_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 * \param srcIP IP of the sender
 * \param dstIP IP of the listening server
 * \param srcPort Port the client should use
 * \param dstPort Port of the server
 * \param ident Identiry of the connection
 * \return void* to BufArray object (give to _getPkts() )
 */
void *HelloBye3_Client_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t srcIP, uint32_t dstIP,
	uint16_t srcPort, uint16_t dstPort, uint64_t ident);

/*! Get the packets to send and free
 *
 * \param obj object returned by HelloBye3_Client_process
 * \param sendPkts Packets to send out (size if given in sendCount)
 * \param freePkts Packets to free (size if given in freeCount)
 */
void HelloBye3_Client_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Process a batch of packets
 *
 * \param obj object returned by HelloBye3_Client_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 * \return void* to BufArray object (give to _getPkts() )
 */
void *HelloBye3_Client_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Free recources used by the state machine
 *
 * \param obj object returned by HelloBye3_Client_init()
 */
void HelloBye3_Client_free(void *obj);
};

/*
 * YCM definition is a workaround for a libclang bug
 * When compiling, YCM should never be set.
 * Set YCM in a libclang based IDE in order to avoid errors
 */
#ifndef YCM
#include "../src/helloBye3.cpp"
#endif

#endif /* HELLOBYE3PROTO_HPP */
