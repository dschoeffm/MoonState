#ifndef HELLOBYE_C
#define HELLOBYE_C

#include "helloByeProto.hpp"

#include <rte_mbuf.h>

extern "C" {

/*! Init a HelloBye server
 *	\return void* to the object (opaque)
 */
void *HelloByeServer_init();

/*! Process a batch of packets
 * \param obj object returned by HelloByeServer_init()
 * \param inPkts incoming packets to process
 * \param inCount number of incoming packets
 * \param sendPkts Packets to send out (caller has to make sure, there is enough room)
 * \param sendCount Number of packets in sendPkts
 * \param freePkts Packets to free (caller has to make sure, there is enough room)
 * \param freeCount Number of packets in freePkts
 */
void HelloByeServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	struct rte_mbuf **sendPkts, unsigned int *sendCount, struct rte_mbuf **freePkts,
	unsigned int *freeCount);

/*! Free recources used by the state machine
 * \param obj object returned by HelloByeServer_init()
 */
void HelloByeServer_free(void *obj);
};

#endif /* HELLOBYE_C */
