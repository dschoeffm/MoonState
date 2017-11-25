#include <cassert>
#include <iostream>

#include "bufArray.hpp"
#include "samplePacket.hpp"

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	unsigned int numPkts = 10;

	SamplePacket **pkts = reinterpret_cast<SamplePacket **>(malloc(numPkts * sizeof(void *)));
	for (unsigned int i = 0; i < numPkts; i++) {
		pkts[i] = new SamplePacket(malloc(100), 100);
	}

	BufArraySM<SamplePacket> *ba = new BufArraySM<SamplePacket>(pkts, numPkts, true);

	// Check some basic stuff
	assert(ba->getFreeCount() == 0);
	assert(ba->getSendCount() == numPkts);

	// Mark one packet as drop
	ba->markDropPkt(1);
	assert(ba->getFreeCount() == 1);
	assert(ba->getSendCount() == (numPkts - 1));

	// Mask the same packet as drop again
	ba->markDropPkt(1);
	assert(ba->getFreeCount() == 1);
	assert(ba->getSendCount() == (numPkts - 1));

	// Inspect, if the correct packet is dropped
	{
		SamplePacket **pktsFree = reinterpret_cast<SamplePacket **>(malloc(sizeof(void *)));
		ba->getFreeBufs(pktsFree);
		assert(pktsFree[0] == pkts[1]);
		free(pktsFree);
	}
	{
		SamplePacket **pktsSend =
			reinterpret_cast<SamplePacket **>(malloc((numPkts - 1) * sizeof(void *)));
		ba->getSendBufs(pktsSend);
		assert(pktsSend[0] == pkts[0]);
		for (unsigned int i = 1; i < (numPkts - 1); i++) {
			assert(pktsSend[i] == pkts[i + 1]);
		}
		free(pktsSend);
	}

	// Make the bufArray grow once
	SamplePacket *firstGrow = new SamplePacket(malloc(100), 100);
	ba->addPkt(firstGrow);
	assert(ba->getSendCount() == 10);
	assert(ba->getFreeCount() == 1);

	// Inspect, if the correct packet is dropped
	{
		SamplePacket **pktsFree = reinterpret_cast<SamplePacket **>(malloc(sizeof(void *)));
		ba->getFreeBufs(pktsFree);
		assert(pktsFree[0] == pkts[1]);
		free(pktsFree);
	}
	{
		SamplePacket **pktsSend =
			reinterpret_cast<SamplePacket **>(malloc(10 * sizeof(void *)));
		ba->getSendBufs(pktsSend);
		assert(pktsSend[0] == pkts[0]);
		for (unsigned int i = 1; i < 9; i++) {
			assert(pktsSend[i] == pkts[i + 1]);
		}
		assert(pktsSend[9] == firstGrow);
		free(pktsSend);
	}

	// Mask another packet as drop
	ba->markDropPkt(5);
	assert(ba->getSendCount() == 9);
	assert(ba->getFreeCount() == 2);

	SamplePacket **pkts2 =
		reinterpret_cast<SamplePacket **>(malloc(numPkts * sizeof(void *)));
	for (unsigned int i = 0; i < numPkts; i++) {
		pkts2[i] = new SamplePacket(malloc(100), 100);
	}

	// Make the bufArray grow once more
	for (unsigned int i = 0; i < numPkts; i++) {
		ba->addPkt(pkts2[i]);
	}

	assert(ba->getFreeCount() == 2);
	assert(ba->getSendCount() == 19);

	// Inspect, if the correct packet is dropped
	{
		SamplePacket **pktsFree =
			reinterpret_cast<SamplePacket **>(malloc(2 * sizeof(void *)));
		ba->getFreeBufs(pktsFree);
		assert(pktsFree[0] == pkts[1]);
		assert(pktsFree[1] == pkts[5]);
		free(pktsFree);
	}
	{
		SamplePacket **pktsSend =
			reinterpret_cast<SamplePacket **>(malloc(ba->getSendCount() * sizeof(void *)));
		ba->getSendBufs(pktsSend);
		assert(pktsSend[0] == pkts[0]);
		assert(pktsSend[1] == pkts[2]);
		assert(pktsSend[2] == pkts[3]);
		assert(pktsSend[3] == pkts[4]);
		assert(pktsSend[4] == pkts[6]);
		assert(pktsSend[5] == pkts[7]);
		assert(pktsSend[6] == pkts[8]);
		assert(pktsSend[7] == pkts[9]);
		assert(pktsSend[8] == firstGrow);
		assert(pktsSend[9] == pkts2[0]);
		assert(pktsSend[10] == pkts2[1]);
		assert(pktsSend[11] == pkts2[2]);
		assert(pktsSend[12] == pkts2[3]);
		assert(pktsSend[13] == pkts2[4]);
		assert(pktsSend[14] == pkts2[5]);
		assert(pktsSend[15] == pkts2[6]);
		assert(pktsSend[16] == pkts2[7]);
		assert(pktsSend[17] == pkts2[8]);
		assert(pktsSend[18] == pkts2[9]);

		free(pktsSend);
	}

	delete (ba);

	for (unsigned int i = 0; i < numPkts; i++) {
		free(pkts[i]->getData());
		free(pkts2[i]->getData());

		delete (pkts[i]);
		delete (pkts2[i]);
	}

	free(pkts);
	free(pkts2);

	return 0;
}
