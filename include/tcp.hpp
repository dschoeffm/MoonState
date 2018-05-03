#ifndef TCP_HPP
#define TCP_HPP

//#include <mutex>
//#include <sstream>
#include <list>
#include <utility>

#include "IPv4_5TupleL2Ident.hpp"
#include "common.hpp"
#include "exceptions.hpp"
#include "headers.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

namespace TCP {
using Identifier = IPv4_5TupleL2Ident<mbuf>;

static constexpr unsigned int MSS = 1460;

/*
 * ===================================
 * Server
 * ===================================
 *
 */

template <class Proto, class ConCtl> class Server;

/*
class ProtoBase {
public:
	virtual void handlePacket(uint8_t *data, uint16_t dataLen, TcpIface &tcpIface) = 0;
};

class ConCtlBase {
public:
	virtual void initSeq(uint32_t seq, uint32_t ack) = 0;
	virtual void handlePacket(uint32_t seq, uint32_t ack) = 0;
	virtual bool reset(uint32_t &seq);
	virtual unsigned int getConWindow() = 0;
};
*/

struct States {
	static constexpr StateID syn_ack = 0;
	static constexpr StateID est = 1;
	static constexpr StateID fin = 2;
	static constexpr StateID ack_fin = 3;
	static constexpr StateID END = 4;
};

template <class Proto, class ConCtl> class Server {
public:
	struct segment {
		uint8_t *dataPtr;
		uint32_t seqNumber;
		uint16_t dataLen;
	};

	struct connection {
		uint32_t seqLocal = 0;
		uint32_t seqRemote = 0;
		std::list<struct segment> dataToSend;
		std::list<struct segment> dataAlreadySent;
//		std::list<struct segment> recvBuffer;
//		uint32_t recvBytes = 0;
		uint16_t sendWindow = 0;
		uint16_t ipID = 0;
		bool closeConnectionAfterSending = false;
		ConCtl conCtl;
		Proto proto;
	};

	class TcpIface {
	private:
		friend class Server<Proto, ConCtl>;
		struct connection &conn;

		TcpIface(struct connection &conn) : conn(conn){};

	public:
		void close() { conn.closeConnectionAfterSending = true; };
		void sendData(uint8_t *data, uint32_t dataLen) {
			/*
			auto oldSize = conn.dataToSend.size();
			conn.dataToSend.resize(oldSize + dataLen);
			memcpy(conn.dataToSend.data() + oldSize, data, dataLen);
			*/
			if (dataLen <= MSS) {
				struct segment seg;

				seg.dataPtr = data;
				seg.dataLen = dataLen;
				seg.seqNumber = conn.seqLocal;
				conn.seqLocal += dataLen;

				conn.dataToSend.push_back(seg);
			} else {
				uint32_t totalLen = 0;

				/*
				 * First segment reuses the given buffer
				 * This might mean to waste memory, but it saves malloc() and free()
				 * calls
				 *
				 * It would also be possible to append to the last non-full segment
				 */
				struct segment segFirst;
				segFirst.dataPtr = data;
				segFirst.dataLen = MSS;
				segFirst.seqNumber = conn.seqLocal;
				conn.seqLocal += MSS;

				conn.dataToSend.push_back(segFirst);

				totalLen += MSS;

				while (totalLen < dataLen) {
					uint32_t curSize = std::min(dataLen - totalLen, MSS);

					struct segment segCur;
					segCur.dataPtr = reinterpret_cast<uint8_t *>(malloc(curSize));
					memcpy(segCur.dataPtr, data + totalLen, curSize);
					segCur.dataLen = curSize;
					segCur.seqNumber = conn.seqLocal;
					conn.seqLocal += curSize;

					conn.dataToSend.push_back(segCur);

					totalLen += curSize;
				}
			}
		};
	};

	static struct connection *factory(Identifier::ConnectionID id);

	static void runSynAck(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
		StateMachine<Identifier, mbuf>::FunIface &funIface);

	static void runEst(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
		StateMachine<Identifier, mbuf>::FunIface &funIface);

	static void runFin(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
		StateMachine<Identifier, mbuf>::FunIface &funIface);

	static void runAckFin(StateMachine<Identifier, mbuf>::State &state, mbuf *pkt,
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

#if 0
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
#endif

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
#ifndef YCM
#include "../src/tcp.cpp"
#endif

#endif /* TCP_HPP */
