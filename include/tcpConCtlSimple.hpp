#include <cstdint>
#include <limits>

//#include "tcp.hpp"

class TcpConCtlSimple /*: public TCP::ConCtlBase */ {
public:
	void initSeq(uint32_t seq, uint32_t ack) {
		(void)seq;
		(void)ack;
	};

	void handlePacket(uint32_t seq, uint32_t ack) {
		(void)seq;
		(void)ack;
	};

	bool reset(uint32_t &seq) {
		(void)seq;
		return false;
	};

	unsigned int getConWindow() { return std::numeric_limits<unsigned int>::max(); };
};
