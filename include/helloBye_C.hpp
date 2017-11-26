#ifndef HELLOBYE_C
#define HELLOBYE_C

#include "helloByeProto.hpp"

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
void *HelloByeServer_init();

/*! Process a batch of packets
 *
 * \param obj object returned by HelloByeServer_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 */
void *HelloByeServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Get the packets to send and free
 *
 * \param obj object returned by HelloByeServer_process
 * \param sendPkts Packets to send out (size if given in sendCount)
 * \param freePkts Packets to free (size if given in freeCount)
 */
void HelloByeServer_getPkts(
	void *obj, struct rte_mbuf **sendPkts, struct rte_mbuf **freePkts);

/*! Free recources used by the state machine
 *
 * \param obj object returned by HelloByeServer_init()
 */
void HelloByeServer_free(void *obj);

/*
 * XXX -------------------------------------------- XXX
 *       Client
 * XXX -------------------------------------------- XXX
 */

/*! Init a HelloBye client
 *
 * \return void* to the object (opaque)
 */
void *HelloByeClient_init();

/*! Configure the client
 *
 * \param srcIP Source IP, will be the same for all connections, host byte order
 * \param dstPort Destination port, will be the same for all connections, host byte order
 */
void HelloByeClient_config(uint32_t srcIP, uint16_t dstPort);

/*! Open a connection to a server
 *
 * \param obj object returned by HelloByeClient_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendCount Number of packets in sendPkts
 * \param freeCount Number of packets in freePkts
 * \param dstIP IP of the listening server
 * \param srcPort Port the client should use
 * \return void* to BufArray object (give to _getPkts() )
 */
void *HelloByeClient_connect(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount, uint32_t dstIP, uint16_t srcPort);

/*! Get the packets to send and free
 *
 * \param obj object returned by HelloByeClient_process
 * \param sendPkts Packets to send out (size if given in sendCount)
 * \param freePkts Packets to free (size if given in freeCount)
 */
void HelloByeClient_getPkts(
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
void *HelloByeClient_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	unsigned int *sendCount, unsigned int *freeCount);

/*! Free recources used by the state machine
 *
 * \param obj object returned by HelloByeClient_init()
 */
void HelloByeClient_free(void *obj);
};

#endif /* HELLOBYE_C */
