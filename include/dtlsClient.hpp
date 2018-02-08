#include <array>
#include <cstdint>
#include <iostream>

#include <openssl/bio.h>
#include <openssl/conf.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "IPv4_5TupleL2Ident.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

namespace DTLS_Client {

struct dtlsClient {
	SSL *ssl;
	BIO *wbio;
	BIO *rbio;
	uint32_t localIP;
	uint32_t remoteIP;
	uint16_t localPort;
	uint16_t remotePort;
	uint16_t counter;
};

/*! The state of the DTLS client
 *
 * These descibe the state IDs for the DTLS client.
 */
struct States {
	static constexpr StateID DOWN = 0;
	static constexpr StateID HANDSHAKE = 1;
	static constexpr StateID ESTABLISHED = 2;
	static constexpr StateID RUN_TEARDOWN = 3;
	static constexpr StateID DELETED = 4;
};

/*! Use this to create the SSL context for creaeteStateData()
 *
 * \return SSL context suitable to create a DTLS client
 */
SSL_CTX *createCTX();

/*! Configure the state machine
 *
 * This functon takes the state machine and configures it according to
 * this implementation of a DTLS client.
 * In pratice, this means, that it will set the functions to be executed
 * within each state.
 *
 * \param sm The state machine to be configured
 */
void configStateMachine(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> &sm);

/*! Create the state of the client
 *
 * \param ctx SSL context, get this from createCTX()
 * \param localIP IP of this host (client)
 * \param remoteIP IP of the peer (server)
 * \param localPort Source port of this client
 * \param remotePort Destination port of the server
 *
 * \return The complete state to kick of a client connection
 */
StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State createStateData(SSL_CTX *ctx,
	uint32_t localIP, uint32_t remoteIP, uint16_t localPort, uint16_t remotePort);

/*
 * The following functions are the state functions, as they should be registered
 * in the StateMachine<>
 */

void initHandshake(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void runHandshake(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void sendData(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void runTeardown(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

}; // namespace DTLS_Client

extern "C" {
/*! Initialize a DTLS client
 *
 * \param dstIP IP of the DTLS test server
 * \param dstPort Port of the DTLS test server
 *
 * \return An opaque object which should be fed into DtlsClient_connect
 */
void *DtlsClient_init(uint32_t dstIP, uint16_t dstPort);

/*! Add one connection to the State Machine
 *
 * \param obj The void* returned from DtlsClient_init()
 * \param inPkts mbufs from MoonGen
 * \param inCount Number of buffers in inPkts (should be 1)
 * \param sendCount This will be set to the number of packets to be sent
 * \param freeCount This will be set to the number of packets to be freed
 * \param srcIP IP which should be used as the source
 * \param srcPort Port which should be used as the source
 *
 * \return Opaque object to be fed into DtlsClient_getPkts
 */
void *DtlsClient_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t srcIP, uint16_t srcPort);

/*! Get the packets from an opaque structure
 *
 * \param obj Return value of DtlsClient_connect() or DtlsClient_process()
 * \param sendPkts Array of packet buffers to be sent
 * \param freePkts Array of packet buffers to be freed
 */
void DtlsClient_getPkts(void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Process incoming packets
 *
 * \param obj Structure returned from DtlsClient_init()
 * \param inPkts The newly arrived packets
 * \param inCount Number of incoming packets
 * \param sendCount Will be set to the number of packets to be sent
 * \param freeCount Will be set to the number of packets to be freed
 */
void *DtlsClient_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Free recources used by the state machine
 *
 * \param obj object returned by DtlsClient_init()
 */
void DtlsClient_free(void *obj);
}
