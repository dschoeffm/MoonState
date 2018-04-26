#ifndef TCP_HPP
#define TCP_HPP

//#include <mutex>
//#include <sstream>

#include "IPv4_5TupleL2Ident.hpp"
#include "common.hpp"
#include "exceptions.hpp"
#include "headers.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

namespace TCP {
using Identifier = IPv4_5TupleL2Ident<mbuf>;

struct connection {
	uint32_t seqLocal;
	uint32_t seqRemote;
};

/*
 * ===================================
 * Server
 * ===================================
 *
 */

namespace Server {

struct States {
	static constexpr StateID syn_ack = 0;
	static constexpr StateID fin = 1;
	static constexpr StateID ack_fin = 2;
	static constexpr StateID END = 3;
};

void *factory(Identifier::ConnectionID id);

void runSynAck(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface);

void runFin(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface);

void runAckFin(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface);

}; // namespace Server

/*
 * ===================================
 * Client
 * ===================================
 *
 */

namespace Client {

struct States {
	static constexpr StateID syn = 0;
	static constexpr StateID ack = 1;
	static constexpr StateID fin = 2;
	static constexpr StateID END = 3;
};

StateMachine<Identifier, mbuf>::State createSyn(
	uint32_t srcIp, uint32_t dstIp, uint16_t srcPort, uint16_t dstPort);

void runAck(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface);

void runFin(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
	StateMachine<Identifier, mbuf>::FunIface &funIface);

} // namespace Client

}; // namespace TCP

/*
 * From here on down is the C interface to the above C++ functions
 */

extern "C" {

/*
 * XXX -------------------------------------------- XXX
 *       Server
 * XXX -------------------------------------------- XXX
 */

/*! Init a TCP server
 *
 *	\return void* to the object (opaque)
 */
void *TCP_Server_init();

/*! Process a batch of packets
 *
 * \param obj object returned by TCPServer_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 */
void *TCP_Server_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Get the packets to send and free
 *
 * \param obj object returned by TCPServer_process
 * \param sendPkts Packets to send out (size if given in sendCount)
 * \param freePkts Packets to free (size if given in freeCount)
 */
void TCP_Server_getPkts(void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Free recources used by the state machine
 *
 * \param obj object returned by TCPServer_init()
 */
void TCP_Server_free(void *obj);

/*
 * XXX -------------------------------------------- XXX
 *       Client
 * XXX -------------------------------------------- XXX
 */

/*! Init a TCP client
 *
 * \return void* to the object (opaque)
 */
void *TCP_Client_init();

/*! Open a connection to a server
 *
 * \param obj object returned by TCP_Client_init()
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
void *TCP_Client_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t srcIP, uint32_t dstIP,
	uint16_t srcPort, uint16_t dstPort);

/*! Get the packets to send and free
 *
 * \param obj object returned by TCP_Client_process
 * \param sendPkts Packets to send out (size if given in sendCount)
 * \param freePkts Packets to free (size if given in freeCount)
 */
void TCP_Client_getPkts(void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Process a batch of packets
 *
 * \param obj object returned by TCP_Client_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 * \return void* to BufArray object (give to _getPkts() )
 */
void *TCP_Client_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Free recources used by the state machine
 *
 * \param obj object returned by TCP_Client_init()
 */
void TCP_Client_free(void *obj);
};

/*
 * YCM definition is a workaround for a libclang bug
 * When compiling, YCM should never be set.
 * Set YCM in a libclang based IDE in order to avoid errors
 */
// Is this include really needed?
// We are not defining a template here
//#ifndef YCM
//#include "../src/tcp.cpp"
//#endif

#endif /* TCP_HPP */
