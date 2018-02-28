#include <cassert>
#include <iostream>

#include "bufArray.hpp"
#include "helloBye2Proto.hpp"
#include "samplePacket.hpp"
#include "stateMachine.hpp"

using Packet = SamplePacket;
using Ident = HelloBye2::Identifier<Packet>;
using SM = StateMachine<Ident, Packet>;

using namespace HelloBye2;
using namespace std;

#define BATCH_SIZE 8
#define BUF_SIZE 100

int main(int argc, char **argv) {
	(void)argc;
	(void)argv;

	Packet **spArrayInit = reinterpret_cast<Packet **>(malloc(sizeof(void *) * BATCH_SIZE));
	for (int i = 0; i < BATCH_SIZE; i++) {
		spArrayInit[i] = new Packet(malloc(BUF_SIZE), BUF_SIZE);
	}

	BufArray<Packet> bufArray(spArrayInit, BATCH_SIZE);

	SM client;
	SM server;

	SM::ConnectionPool clientPool;
	SM::ConnectionPool serverPool;

	client.setConnectionPool(&clientPool);
	server.setConnectionPool(&serverPool);

	client.registerEndStateID(Client::States::Terminate);
	client.registerFunction(Client::States::Hello, Client::Hello<Ident, Packet>::run);
	client.registerFunction(Client::States::Bye, Client::Bye<Ident, Packet>::run);
	client.registerFunction(Client::States::RecvBye, Client::RecvBye<Ident, Packet>::run);

	server.registerEndStateID(Server::States::Terminate);
	server.registerStartStateID(Server::States::Hello, Server::Hello<Ident, Packet>::factory);
	server.registerFunction(Server::States::Hello, Server::Hello<Ident, Packet>::run);
	server.registerFunction(Server::States::Bye, Server::Bye<Ident, Packet>::run);

	// HelloBye2ClientConfig::createInstance();

	uint64_t counter = 0;

	assert(client.getStateTableSize() == 0);
	assert(server.getStateTableSize() == 0);

	cout << "-----------------------------------" << endl;
	cout << "Filling the batch now" << endl;
	cout << "-----------------------------------" << endl;

	// Fill the batch
	for (int i = 0; i < BATCH_SIZE; i++) {
		auto hello = new Client::Hello<Ident, Packet>(0x0001, 1025, counter);
		BufArray<Packet> clientBA(&spArrayInit[i], 1, true);
		SM::State state(Client::States::Hello, hello);
		Ident::ConnectionID cID(counter);

		client.addState(cID, state, &clientBA);
		assert(clientBA.getSendCount() == 1);
		assert(clientBA.getFreeCount() == 0);

		counter++;
	}

	// The new connections are all not bound to the core/instance yet
	assert(client.getStateTableSize() == 0);
	assert(server.getStateTableSize() == 0);

	cout << "-----------------------------------" << endl;
	cout << "Pass batch to the server (hello msg)" << endl;
	cout << "-----------------------------------" << endl;

	// Work the batch in the server
	server.runPktBatch(&bufArray);
	assert(bufArray.getSendCount() == BATCH_SIZE);
	assert(bufArray.getFreeCount() == 0);
	assert(client.getStateTableSize() == 0);
	assert(server.getStateTableSize() == BATCH_SIZE);

	cout << "-----------------------------------" << endl;
	cout << "Pass batch to the client (hello msg)" << endl;
	cout << "-----------------------------------" << endl;

	// Work the batch in the client
	client.runPktBatch(&bufArray);
	assert(bufArray.getSendCount() == BATCH_SIZE);
	assert(bufArray.getFreeCount() == 0);
	assert(client.getStateTableSize() == BATCH_SIZE);
	assert(server.getStateTableSize() == BATCH_SIZE);

	cout << "-----------------------------------" << endl;
	cout << "Pass batch to the server (bye msg)" << endl;
	cout << "-----------------------------------" << endl;

	// Work the batch in the server
	server.runPktBatch(&bufArray);
	assert(bufArray.getSendCount() == BATCH_SIZE);
	assert(bufArray.getFreeCount() == 0);
	assert(client.getStateTableSize() == BATCH_SIZE);
	assert(server.getStateTableSize() == 0);

	cout << "-----------------------------------" << endl;
	cout << "Pass batch to the client (bye msg)" << endl;
	cout << "-----------------------------------" << endl;

	// Work the batch in the client
	client.runPktBatch(&bufArray);
	assert(bufArray.getSendCount() == 0);
	assert(bufArray.getFreeCount() == BATCH_SIZE);
	assert(client.getStateTableSize() == 0);
	assert(server.getStateTableSize() == 0);

	for (int i = 0; i < BATCH_SIZE; i++) {
		delete (spArrayInit[i]);
	}

	cout << "-----------------------------------" << endl;
	cout << "Test ran through without any problems" << endl;
	cout << "-----------------------------------" << endl;

	return 0;
};
