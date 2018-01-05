#include <cassert>
#include <cstdint>
#include <iostream>
#include <thread>

#include "bufArray.hpp"
#include "helloBye2_C.hpp"
#include "mbuf.hpp"
#include "spinlock.hpp"
#include "stateMachine.hpp"

#undef likely
#undef unlikely

using Packet = mbuf;
using Ident = HelloBye2::Identifier<Packet>;
using SM = StateMachine<Ident, Packet>;

using namespace HelloBye2;
using namespace std;

#if 1

#include "blockingconcurrentqueue.h"
using namespace moodycamel;

#else

template <typename T> class BlockingConcurrentQueue {
private:
	// std::mutex mtx;
	SpinLockCLSize mtx;
	std::queue<T> q;

public:
	BlockingConcurrentQueue(){};
	BlockingConcurrentQueue(int i) { (void)i; };

	void enqueue_bulk(T *ins, unsigned int insCount) {
		// std::lock_guard<std::mutex> lock(mtx);
		std::lock_guard<SpinLockCLSize> lock(mtx);

		for (unsigned int i = 0; i < insCount; i++) {
			q.push(ins[i]);
		}
	};
	size_t try_dequeue_bulk(T *outs, unsigned int outsCount) {
		// std::lock_guard<std::mutex> lock(mtx);
		std::lock_guard<SpinLockCLSize> lock(mtx);

		size_t retCount = 0;
		while (retCount < outsCount) {
			if (!q.empty()) {
				outs[retCount] = q.front();
				q.pop();
			} else {
				return retCount;
			}
		}
		return retCount;
	};
};
#endif

#define BATCH_SIZE 64

void clientConnector(atomic<bool> *run, BlockingConcurrentQueue<rte_mbuf *> *pipeCS,
	SM::ConnectionPool *connPool) {
	void *obj = HelloBye2_Client_init();
	SM *obj_sm = reinterpret_cast<SM *>(obj);

	obj_sm->setConnectionPool(connPool);

	uint64_t ident = 0;
	struct rte_mbuf **pkts =
		reinterpret_cast<struct rte_mbuf **>(malloc(sizeof(mbuf *) * BATCH_SIZE));

	while (run->load()) {
		for (int i = 0; i < BATCH_SIZE; i++) {
			pkts[i] = reinterpret_cast<struct rte_mbuf *>(malloc(sizeof(mbuf)));
			memset(pkts[i], 0, sizeof(struct rte_mbuf));
			pkts[i]->buf_addr = malloc(100);
			pkts[i]->buf_len = 100;
			pkts[i]->data_len = 100;
			memset(pkts[i]->buf_addr, 0, 100);
		}

		unsigned int fbc, sbc;
		struct rte_mbuf **sendBufsAll =
			reinterpret_cast<struct rte_mbuf **>(malloc(sizeof(mbuf *) * BATCH_SIZE));

		unsigned int sbcTotal = 0;

		for (int i = 0; i < BATCH_SIZE; i++) {
			void *ba_v = HelloBye2_Client_connect(
				obj, &(pkts[i]), 1, &sbc, &fbc, 0x0, 0x1337, ident + i);

			struct rte_mbuf **freeBufs =
				reinterpret_cast<struct rte_mbuf **>(malloc(sizeof(mbuf *) * fbc));

			HelloBye2_Client_getPkts(ba_v, &sendBufsAll[i], freeBufs);

			sbcTotal += sbc;

			for (unsigned int i = 0; i < fbc; i++) {
				free(freeBufs[i]);
			}

			free(freeBufs);
		}

		pipeCS->enqueue_bulk(sendBufsAll, sbcTotal);

		free(sendBufsAll);

		ident += BATCH_SIZE;
	}
	free(pkts);

	HelloBye2_Client_free(obj);
};

void clientReflector(atomic<bool> *run, BlockingConcurrentQueue<rte_mbuf *> *pipeCS,
	BlockingConcurrentQueue<rte_mbuf *> *pipeSC, SM::ConnectionPool *connPool) {
	void *obj = HelloBye2_Client_init();
	SM *obj_sm = reinterpret_cast<SM *>(obj);

	obj_sm->setConnectionPool(connPool);
	struct rte_mbuf **pkts = reinterpret_cast<struct rte_mbuf **>(
		malloc(sizeof(struct rte_mbuf *) * BATCH_SIZE * 2));

	unsigned int sbc, fbc;

	struct rte_mbuf **sbufs = reinterpret_cast<struct rte_mbuf **>(
		malloc(sizeof(struct rte_mbuf *) * BATCH_SIZE * 2));
	struct rte_mbuf **fbufs = reinterpret_cast<struct rte_mbuf **>(
		malloc(sizeof(struct rte_mbuf *) * BATCH_SIZE * 2));
	unsigned int sbufsS = BATCH_SIZE * 2;
	unsigned int fbufsS = BATCH_SIZE * 2;

	while (run->load()) {
		size_t inCount = pipeSC->try_dequeue_bulk(pkts, BATCH_SIZE * 2);

		if (inCount > 0) {
			void *ba = HelloBye2_Client_process(obj, pkts, inCount, &sbc, &fbc);

			if (sbc > sbufsS) {
				free(sbufs);
				sbufs = reinterpret_cast<struct rte_mbuf **>(
					malloc(sizeof(struct rte_mbuf *) * sbc));
				sbufsS = sbc;
			}

			if (fbc > fbufsS) {
				free(fbufs);
				fbufs = reinterpret_cast<struct rte_mbuf **>(
					malloc(sizeof(struct rte_mbuf *) * fbc));
				fbufsS = fbc;
			}

			HelloBye2_Client_getPkts(ba, sbufs, fbufs);

			for (unsigned int i = 0; i < fbc; i++) {
				free(fbufs[i]->buf_addr);
				free(fbufs[i]);
			}

			pipeCS->enqueue_bulk(sbufs, sbc);
		}
	}

	free(sbufs);
	free(fbufs);

	free(pkts);

	HelloBye2_Client_free(obj);
};

void serverReflector(atomic<bool> *run, BlockingConcurrentQueue<rte_mbuf *> *pipeCS,
	BlockingConcurrentQueue<rte_mbuf *> *pipeSC) {
	void *obj = HelloBye2_Server_init();

	struct rte_mbuf **pkts = reinterpret_cast<struct rte_mbuf **>(
		malloc(sizeof(struct rte_mbuf *) * BATCH_SIZE * 2));

	unsigned int sbc, fbc;

	struct rte_mbuf **sbufs = reinterpret_cast<struct rte_mbuf **>(
		malloc(sizeof(struct rte_mbuf *) * BATCH_SIZE * 2));
	struct rte_mbuf **fbufs = reinterpret_cast<struct rte_mbuf **>(
		malloc(sizeof(struct rte_mbuf *) * BATCH_SIZE * 2));
	unsigned int sbufsS = BATCH_SIZE * 2;
	unsigned int fbufsS = BATCH_SIZE * 2;

	while (run->load()) {
		size_t inCount = pipeCS->try_dequeue_bulk(pkts, BATCH_SIZE * 2);

		if (inCount > 0) {
			void *ba = HelloBye2_Server_process(obj, pkts, inCount, &sbc, &fbc);

			if (sbc > sbufsS) {
				free(sbufs);
				sbufs = reinterpret_cast<struct rte_mbuf **>(
					malloc(sizeof(struct rte_mbuf *) * sbc));
				sbufsS = sbc;
			}

			if (fbc > fbufsS) {
				free(fbufs);
				fbufs = reinterpret_cast<struct rte_mbuf **>(
					malloc(sizeof(struct rte_mbuf *) * fbc));
				fbufsS = fbc;
			}

			HelloBye2_Server_getPkts(ba, sbufs, fbufs);

			for (unsigned int i = 0; i < fbc; i++) {
				free(fbufs[i]->buf_addr);
				free(fbufs[i]);
			}

			pipeSC->enqueue_bulk(sbufs, sbc);
		}
	}

	free(sbufs);
	free(fbufs);

	free(pkts);

	HelloBye2_Server_free(obj);
};

void usage(string str) { cout << "Usage: " << str << " (time)" << endl; }

int main(int argc, char **argv) {
	int time = 3;
	if (argc > 1) {
		time = atoi(argv[1]);
	}

	SM::ConnectionPool cp;
	SM::ConnectionPool cp2;

	atomic<bool> runCC;
	runCC.store(true);
	atomic<bool> runCR;
	runCR.store(true);
	atomic<bool> runSR;
	runSR.store(true);

	BlockingConcurrentQueue<struct rte_mbuf *> pipeCS(100000);
	BlockingConcurrentQueue<struct rte_mbuf *> pipeSC(100000);

	HelloBye2_Client_config(1, 1338);

	std::cout << "--- Starting threads ---" << std::endl;

	thread clientConnector_th(clientConnector, &runCC, &pipeCS, &cp);
	thread clientReflector_th(clientReflector, &runCR, &pipeCS, &pipeSC, &cp);
	thread serverReflector_th(serverReflector, &runSR, &pipeCS, &pipeSC);

	std::this_thread::sleep_for(std::chrono::seconds(time));

	std::cout << "--- Stopping ClientConnector ---" << std::endl;

	runCC.store(false);
	clientConnector_th.join();

	std::this_thread::sleep_for(std::chrono::seconds(2));

	std::cout << "--- Stopping ClientReflector ---" << std::endl;

	runCR.store(false);
	clientReflector_th.join();

	std::cout << "--- Client threads stopped ---" << std::endl;

	/*

	runCC.store(true);
	runCR.store(true);

	std::cout << "--- Starting new client threads ---" << std::endl;

	thread clientConnector_th2(clientConnector, &runCC, &pipeCS, &cp2);
	thread clientReflector_th2(clientReflector, &runCR, &pipeCS, &pipeSC, &cp2);

	std::this_thread::sleep_for(std::chrono::seconds(time));

	std::cout << "--- Stopping ClientConnector ---" << std::endl;

	runCC.store(false);
	clientConnector_th2.join();

	std::this_thread::sleep_for(std::chrono::seconds(1));

	std::cout << "--- Stopping ClientReflector ---" << std::endl;

	runCR.store(false);
	clientReflector_th2.join();

	*/
	std::cout << "--- Stopping ServerReflector ---" << std::endl;

	runSR.store(false);
	serverReflector_th.join();

	return 0;
};
