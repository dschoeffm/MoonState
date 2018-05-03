#include <cstdint>
#include <limits>

//#include "tcp.hpp"

class TcpConCtlSimple /*: public TCP::ConCtlBase */ {
private:
	uint32_t lastAck;
	uint32_t alreadySeen;
public:
	void initSeq(uint32_t seq, uint32_t ack) {
		(void)seq;
		lastAck = ack;
	};

	void handlePacket(uint32_t seq, uint32_t ack) {
		if((lastAck == ack) && (ack < seq)){
			alreadySeen++;
//			std::cout << "ACK already seen: " << ack << " seq: " << seq << std::endl;
		} else {
//			std::cout << "New ACK: " << ack  << " seq: " << seq << std::endl;
			lastAck = ack;
			alreadySeen = 0;
		}
	};

	bool reset(uint32_t &seq) {
		if(alreadySeen >= 3){
//			std::cout << "Resetting to: " << lastAck << std::endl;
			seq = lastAck;
			return true;
		}
		return false;
	};

	unsigned int getConWindow() { return std::numeric_limits<unsigned int>::max(); };
};
