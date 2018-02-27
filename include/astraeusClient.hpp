#ifndef ASTRAEUSCLIENT_HPP
#define ASTRAEUSCLIENT_HPP

#include <array>
#include <cstdint>
#include <iostream>

#include <rte_mempool.h>

#include "cryptoProto.hpp"

#include "IPv4_5TupleL2Ident.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

namespace Astraeus_Client {

struct astraeusClient {
	AstraeusProto::protoHandle handle;
	uint32_t localIP;
	uint32_t remoteIP;
	uint16_t localPort;
	uint16_t remotePort;
};

/*! The state of the Astraeus client
 *
 * These descibe the state IDs for the Astraeus client.
 */
struct States {
	static constexpr StateID DOWN = 0;
	static constexpr StateID HANDSHAKE = 1;
	static constexpr StateID ESTABLISHED = 2;
	static constexpr StateID RUN_TEARDOWN = 3;
	static constexpr StateID DELETED = 4;
};

/*! Use this to create an identity context for creaeteStateData()
 *
 * \return identity suitable to create an Astraeus client
 */
AstraeusProto::identityHandle *createIdentity();

/*! Configure the state machine
 *
 * This functon takes the state machine and configures it according to
 * this implementation of a Astraeus client.
 * In pratice, this means, that it will set the functions to be executed
 * within each state.
 *
 * \param sm The state machine to be configured
 * \param mp MemPool to allocate extra mbufs from
 */
void configStateMachine(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf> &sm, rte_mempool *mp);

/*! Create the state of the client
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

}; // namespace Astraeus_Client

extern "C" {
/*! Initialize an Astraeus client
 *
 * \param dstIP IP of the Astraeus test server
 * \param dstPort Port of the Astraeus test server
 *
 * \return An opaque object which should be fed into AstraeusClient_connect
 */
void *AstraeusClient_init(uint32_t dstIP, uint16_t dstPort);

/*! Add one connection to the State Machine
 *
 * \param obj The void* returned from AstraeusClient_init()
 * \param inPkts mbufs from MoonGen
 * \param inCount Number of buffers in inPkts (should be 1)
 * \param sendCount This will be set to the number of packets to be sent
 * \param freeCount This will be set to the number of packets to be freed
 * \param srcIP IP which should be used as the source
 * \param srcPort Port which should be used as the source
 *
 * \return Opaque object to be fed into AstraeusClient_getPkts
 */
void *AstraeusClient_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t srcIP, uint16_t srcPort);

/*! Get the packets from an opaque structure
 *
 * \param obj Return value of AstraeusClient_connect() or AstraeusClient_process()
 * \param sendPkts Array of packet buffers to be sent
 * \param freePkts Array of packet buffers to be freed
 */
void AstraeusClient_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Process incoming packets
 *
 * \param obj Structure returned from AstraeusClient_init()
 * \param inPkts The newly arrived packets
 * \param inCount Number of incoming packets
 * \param sendCount Will be set to the number of packets to be sent
 * \param freeCount Will be set to the number of packets to be freed
 */
void *AstraeusClient_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Free recources used by the state machine
 *
 * \param obj object returned by AstraeusClient_init()
 */
void AstraeusClient_free(void *obj);
}

#endif /* ASTRAEUSCLIENT_HPP */
