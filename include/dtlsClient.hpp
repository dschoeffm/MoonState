#include <array>
#include <cstdint>
#include <iostream>
#include <openssl/conf.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

#include "IPv4_5TupleL2Ident.hpp"
#include "mbuf.hpp"
#include "stateMachine.hpp"

namespace DTLS_Client {

struct dtlsClient {
	SSL *ssl;
	BIO *wbio;
	BIO *rbio;
	uint32_t localIP;
	uint32_t remoteIP;
	uint16_t localPort;
	uint16_t remotePort;
	uint16_t counter;
	std::array<uint8_t, 6> localMac;
	std::array<uint8_t, 6> remoteMac;
};

/*! The state of the DTLS client
 *
 * These descibe the state IDs for the DTLS client.
 */
struct States {
	static constexpr StateID DOWN = 0;
	static constexpr StateID HANDSHAKE = 1;
	static constexpr StateID ESTABLISHED = 2;
	static constexpr StateID INIT_TEARDOWN = 3;
	static constexpr StateID RUN_TEARDOWN = 4;
	static constexpr StateID DELETED = 5;
};

/*! Use this to create the SSL context for creaeteStateData()
 *
 * \return SSL context suitable to create a DTLS client
 */
SSL_CTX *createCTX();

/*! Create the state of the client
 *
 * \param ctx SSL context, get this from createCTX()
 * \param localIP IP of this host (client)
 * \param remoteIP IP of the peer (server)
 * \param localPort Source port of this client
 * \param remotePort Destination port of the server
 * \param localMac MAC address of the client NIC
 * \param remoteMac MAC address of the next hop
 *
 * \return The complete state to kick of a client connection
 */
StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State *createStateData(SSL_CTX *ctx,
	uint32_t localIP, uint32_t remoteIP, uint16_t localPort, uint16_t remotePort,
	std::array<uint8_t, 6> localMac, std::array<uint8_t, 6> remoteMac);

/*
 * The following functions are the state functions, as they should be registered
 * in the StateMachine<>
 */

void initHandshake(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void runHandshake(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void sendData(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void startTeardown(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

void runTeardown(StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::State &state, mbuf *,
	StateMachine<IPv4_5TupleL2Ident<mbuf>, mbuf>::FunIface &funIface);

}; // namespace DTLS_Client
