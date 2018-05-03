#ifndef ASTRAEUSSERVER_HPP
#define ASTRAEUSSERVER_HPP

#include <array>
#include <cstdint>
#include <iostream>

#include <rte_mempool.h>

#include "cryptoProto.hpp"

#include "IPv4_5TupleL2Ident.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

namespace Astraeus_Server {

struct astraeusClient {
	AstraeusProto::protoHandle handle;
	uint32_t localIP;
	uint32_t remoteIP;
	uint16_t localPort;
	uint16_t remotePort;
};

/*! The state of the Astraeus server
 *
 * These descibe the state IDs for the Astraeus server.
 */
struct States {
	static constexpr StateID HANDSHAKE = 0;
	static constexpr StateID ESTABLISHED = 1;
	static constexpr StateID RUN_TEARDOWN = 2;
	static constexpr StateID DELETED = 3;
};

void *factory(IPv4_5TupleL2Ident<mbuf>::ConnectionID id);

/*! Use this to create an identity context for creaeteStateData()
 *
 * \return identity suitable to create an Astraeus server
 */
AstraeusProto::identityHandle *createIdentity();

/*! Configure the state machine
 *
 * This functon takes the state machine and configures it according to
 * this implementation of a Astraeus server.
 * In pratice, this means, that it will set the functions to be executed
 * within each state.
 *
 * \param sm The state machine to be configured
 * \param mp MemPool to allocate extra mbufs from
 */
void configStateMachine(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> &sm, rte_mempool *mp);

/*! Create the state of the server
 *
 * \param ident identity handle, get this from createIdentity()
 * \param localIP IP of this host (client)
 * \param remoteIP IP of the peer (server)
 * \param localPort Source port of this client
 * \param remotePort Destination port of the server
 *
 * \return The complete state to kick of a client connection
 */
StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State createStateData(
	AstraeusProto::identityHandle *ident, uint32_t localIP, uint32_t remoteIP,
	uint16_t localPort, uint16_t remotePort);

/*
 * The following functions are the state functions, as they should be registered
 * in the StateMachine<>
 */

void initHandshakeNoTransition(
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *);

void initHandshake(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void runHandshake(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void sendData(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void runTeardown(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

}; // namespace Astraeus_Server

extern "C" {
/*! Initialize an Astraeus server
 *
 * \return An opaque object which should be fed into AstraeusServer_process
 */
void *AstraeusServer_init();

/*! Get the packets from an opaque structure
 *
 * \param obj Return value of AstraeusServer_process()
 * \param sendPkts Array of packet buffers to be sent
 * \param freePkts Array of packet buffers to be freed
 */
void AstraeusServer_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Process incoming packets
 *
 * \param obj Structure returned from AstraeusServer_init()
 * \param inPkts The newly arrived packets
 * \param inCount Number of incoming packets
 * \param sendCount Will be set to the number of packets to be sent
 * \param freeCount Will be set to the number of packets to be freed
 */
void *AstraeusServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Free recources used by the state machine
 *
 * \param obj object returned by AstraeusServer_init()
 */
void AstraeusServer_free(void *obj);
}

#endif /* ASTRAEUSSERVER_HPP */
