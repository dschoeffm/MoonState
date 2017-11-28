#ifndef HELLOBYE_C
#define HELLOBYE_C

#include "helloBye2Proto.hpp"

#include <rte_mbuf.h>

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
void *HelloBye2_Server_init();

/*! Process a batch of packets
 *
 * \param obj object returned by HelloByeServer_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 */
void *HelloBye2_Server_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Get the packets to send and free
 *
 * \param obj object returned by HelloByeServer_process
 * \param sendPkts Packets to send out (size if given in sendCount)
 * \param freePkts Packets to free (size if given in freeCount)
 */
void HelloBye2_Server_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Free recources used by the state machine
 *
 * \param obj object returned by HelloByeServer_init()
 */
void HelloBye2_Server_free(void *obj);

/*
 * XXX -------------------------------------------- XXX
 *       Client
 * XXX -------------------------------------------- XXX
 */

/*! Init a HelloBye client
 *
 * \return void* to the object (opaque)
 */
void *HelloBye2_Client_init();

/*! Configure the client
 *
 * \param srcIP Source IP, will be the same for all connections, host byte order
 * \param dstPort Destination port, will be the same for all connections, host byte order
 */
void HelloBye2_Client_config(uint32_t srcIP, uint16_t dstPort);

/*! Open a connection to a server
 *
 * \param obj object returned by HelloByeClient_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 * \param dstIP IP of the listening server
 * \param srcPort Port the client should use
 * \param ident Identiry of the connection
 * \return void* to BufArray object (give to _getPkts() )
 */
void *HelloBye2_Client_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t dstIP, uint16_t srcPort,
	uint64_t ident);

/*! Get the packets to send and free
 *
 * \param obj object returned by HelloByeClient_process
 * \param sendPkts Packets to send out (size if given in sendCount)
 * \param freePkts Packets to free (size if given in freeCount)
 */
void HelloBye2_Client_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Process a batch of packets
 *
 * \param obj object returned by HelloByeClient_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 * \return void* to BufArray object (give to _getPkts() )
 */
void *HelloBye2_Client_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Free recources used by the state machine
 *
 * \param obj object returned by HelloByeClient_init()
 */
void HelloBye2_Client_free(void *obj);
};

#endif /* HELLOBYE_C */
