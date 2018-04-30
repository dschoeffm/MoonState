#include <string>

#include "tcp.hpp"

template <class ConCtl> class TcpProtoJoke /*: public TCP::ProtoBase */ {
public:
	void handlePacket(uint8_t *data, uint16_t dataLen,
		typename TCP::Server<TcpProtoJoke, ConCtl>::TcpIface &tcpIface) {
		/*
		(void)data;
		(void)dataLen;
		std::string str("The good thing about TCP jokes is, that you always get them.");
		tcpIface.sendData(reinterpret_cast<const uint8_t *>(str.c_str()), str.length());
		*/
		tcpIface.sendData(data, dataLen);
	};
};
