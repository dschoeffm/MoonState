#ifndef HELLOBYE_C
#define HELLOBYE_C

#include "helloByeProto.hpp"

#include <rte_mbuf.h>

extern "C" {
void *HelloByeServer_init();
void HelloByeServer_process(void *obj, struct rte_mbuf **inPkts, unsigned int inCount,
	struct rte_mbuf **sendPkts, unsigned int *sendCount, struct rte_mbuf **freePkts,
	unsigned int *freeCount);
void HelloByeServer_free(void *obj);
};

#endif /* HELLOBYE_C */
