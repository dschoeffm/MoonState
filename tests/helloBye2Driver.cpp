#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <cassert>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#define BUFLEN 100

using namespace std;

struct __attribute__((packed)) msg {
	uint64_t ident;
	uint8_t role;
	uint8_t msg;
	uint8_t cookie;

	static constexpr uint8_t ROLE_CLIENT = 0;
	static constexpr uint8_t ROLE_SERVER = 1;

	static constexpr uint8_t MSG_HELLO = 0;
	static constexpr uint8_t MSG_BYE = 0;
};

void client(int fd, struct sockaddr_in server) {
	struct msg msg;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	uint8_t cookieClient = rand();
	uint64_t ident = rand();

	// Send first message
	msg.ident = ident;
	msg.role = msg::ROLE_CLIENT;
	msg.msg = msg::MSG_HELLO;
	msg.cookie = cookieClient;

	if (sendto(fd, &msg, sizeof(struct msg), 0, (const struct sockaddr *)&server, addrlen) ==
		-1) {
		cout << "client: sendto failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Hello message away, Cookie: " << static_cast<int>(cookieClient) << endl;

	// Receive first message
	if (recvfrom(fd, &msg, sizeof(struct msg), 0, NULL, NULL) < 0) {
		cout << "client: recvfrom failed: " << std::strerror(errno) << endl;
		abort();
	}

	if ((msg.ident != ident) || (msg.msg != msg::MSG_HELLO) ||
		(msg.role != msg::ROLE_SERVER)) {
		cout << "client: server hello failed" << endl;
		abort();
	}
	uint8_t cookieServer = msg.cookie;

	cout << "Server hello received, Cookie: " << static_cast<int>(cookieServer) << endl;

	// Send bye message
	msg.role = msg::ROLE_CLIENT;
	msg.ident = ident;
	msg.msg = msg::MSG_BYE;
	msg.cookie = cookieServer;

	if (sendto(fd, &msg, sizeof(struct msg), 0, (const struct sockaddr *)&server, addrlen) ==
		-1) {
		cout << "client: sendto failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Bye message away" << endl;

	// Receive bye message
	if (recvfrom(fd, &msg, sizeof(struct msg), 0, NULL, NULL) < 0) {
		cout << "client: recvfrom failed: " << std::strerror(errno) << endl;
		abort();
	}

	if ((msg.ident != ident) || (msg.msg != msg::MSG_BYE) || (msg.role != msg::ROLE_SERVER)) {
		cout << "client: server bye failed" << endl;
		abort();
	}

	if (cookieClient != msg.cookie) {
		cout << "client: server sent me the wrong cookie... I want chocolate..." << endl;
		abort();
	}

	cout << "Server bye received" << endl;
};

void server(int fd, struct sockaddr_in server) {
	socklen_t addrlen = sizeof(struct sockaddr_in);

	struct msg msg;

	//	int optval = 1;
	//	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

	struct sockaddr_in client;
	unsigned int clen = sizeof(client);

	if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		cout << "server: bind() failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Waiting for client" << endl;

	uint8_t cookieServer = rand();

	// Receive client hello
	if (recvfrom(fd, &msg, sizeof(struct msg), 0, (struct sockaddr *)&client, &clen) < 0) {
		cout << "server: recvfrom failed: " << std::strerror(errno) << endl;
	}

	uint64_t ident = msg.ident;

	if ((msg.msg != msg::MSG_HELLO) || (msg.role != msg::ROLE_CLIENT)) {
		cout << "client: server hello failed" << endl;
		abort();
	}

	uint8_t cookieClient = msg.cookie;

	cout << "Client hello received, Cookie:" << static_cast<int>(cookieClient) << endl;
	cout << "Received packet from: " << inet_ntoa(client.sin_addr) << ":"
		 << ntohs(client.sin_port) << endl;
	assert(client.sin_family == AF_INET);

	// Send first message
	msg.role = msg::ROLE_SERVER;
	msg.msg = msg::MSG_HELLO;
	msg.cookie = cookieServer;
	if (sendto(fd, &msg, sizeof(struct msg), 0, (const struct sockaddr *)&client,
			sizeof(client)) == -1) {
		cout << "server: sendto failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Hello message away, Cookie:" << static_cast<int>(cookieServer) << endl;

	// Receive client bye
	if (recvfrom(fd, &msg, sizeof(struct msg), 0, (struct sockaddr *)&client, &clen) < 0) {
		cout << "server: recvfrom failed: " << std::strerror(errno) << endl;
	}

	if ((msg.ident != ident) || (msg.msg != msg::MSG_BYE) || (msg.role != msg::ROLE_CLIENT)) {
		cout << "client: server hello failed" << endl;
		abort();
	}

	if (cookieServer != msg.cookie) {
		cout << "server: client sent me the wrong cookie... I want vanilla..." << endl;
		abort();
	}

	cout << "Client bye received" << endl;

	// Send server bye
	msg.role = msg::ROLE_SERVER;
	msg.ident = ident;
	msg.msg = msg::MSG_BYE;
	msg.cookie = cookieClient;
	if (sendto(fd, &msg, sizeof(struct msg), 0, (const struct sockaddr *)&client, addrlen) ==
		-1) {
		cout << "server: sendto failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Server bye sent" << endl;
};

void printUsage(string str) {
	cout << "Usage: " << str << " <options>" << endl;
	cout << "Options:" << endl;
	cout << "\t-c <IP> <Port>" << endl;
	cout << "\t-s <Port>" << endl;
	exit(0);
};

int main(int argc, char **argv) {

	int fd;
	struct sockaddr_in sa;
	bool c;
	uint16_t port;

	srand(time(NULL));

	memset((char *)&sa, 0, sizeof(struct sockaddr_in));
	sa.sin_family = AF_INET;

	if (argc < 3) {
		printUsage(string(argv[0]));
	}

	if (strncmp("-c", argv[1], 2) == 0) {
		c = true;

		if (inet_aton(argv[2], &sa.sin_addr) == 0) {
			printUsage(string(argv[0]));
		}
		port = atoi(argv[3]);

	} else if (strncmp("-s", argv[1], 2) == 0) {
		c = false;
		port = atoi(argv[2]);
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
	} else {
		printUsage(string(argv[0]));
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		cout << "socket() failed" << endl;
		abort();
	}

	sa.sin_port = htons(port);

	if (c) {
		client(fd, sa);
	} else {
		server(fd, sa);
	}

	return 0;
}
