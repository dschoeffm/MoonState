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

void client(int fd, struct sockaddr_in server) {
	char buf[BUFLEN];
	socklen_t addrlen = sizeof(struct sockaddr_in);

	// Send first message
	memset(buf, 0, BUFLEN);
	sprintf(buf, "CLIENT HELLO");
	if (sendto(fd, buf, BUFLEN, 0, (const struct sockaddr *)&server, addrlen) == -1) {
		cout << "client: sendto failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Hello message away" << endl;

	// Receive first message
	memset(buf, 0, BUFLEN);
	if (recvfrom(fd, buf, BUFLEN, 0, NULL, NULL) < 0) {
		cout << "client: recvfrom failed: " << std::strerror(errno) << endl;
		abort();
	}

	if (strncmp(buf, "SERVER HELLO", 12) != 0) {
		cout << "client: server hello failed: " << string(buf) << endl;
		abort();
	}

	cout << "Server hello received" << endl;

	// Send bye message
	memset(buf, 0, BUFLEN);
	sprintf(buf, "CLIENT BYE");
	if (sendto(fd, buf, BUFLEN, 0, (const struct sockaddr *)&server, addrlen) == -1) {
		cout << "client: sendto failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Bye message away" << endl;

	// Receive bye message
	memset(buf, 0, BUFLEN);
	if (recvfrom(fd, buf, BUFLEN, 0, NULL, NULL) < 0) {
		cout << "client: recvfrom failed: " << std::strerror(errno) << endl;
		abort();
	}

	if (strncmp(buf, "SERVER BYE", 10) != 0) {
		cout << "client: server bye failed: " << string(buf) << endl;
		abort();
	}

	cout << "Server bye received" << endl;
};

void server(int fd, struct sockaddr_in server) {
	char buf[BUFLEN];
	memset(buf, 0, BUFLEN);
	socklen_t addrlen = sizeof(struct sockaddr_in);

	//	int optval = 1;
	//	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

	struct sockaddr_in client;
	unsigned int clen = sizeof(client);

	if (bind(fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
		cout << "server: bind() failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Waiting for client" << endl;

	// Receive client hello
	if (recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&client, &clen) < 0) {
		cout << "server: recvfrom failed: " << std::strerror(errno) << endl;
	}

	if (strncmp(buf, "CLIENT HELLO", 12) != 0) {
		cout << "server: client hello failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Client hello received" << endl;
	cout << "Received packet from: " << inet_ntoa(client.sin_addr) << ":"
		 << ntohs(client.sin_port) << endl;
	assert(client.sin_family == AF_INET);

	// Send first message
	memset(buf, 0, BUFLEN);
	sprintf(buf, "SERVER HELLO");
	if (sendto(fd, buf, BUFLEN, 0, (const struct sockaddr *)&client, sizeof(client)) == -1) {
		cout << "server: sendto failed: " << std::strerror(errno) << endl;
		abort();
	}

	cout << "Hello message away" << endl;

	// Receive client bye
	if (recvfrom(fd, buf, BUFLEN, 0, (struct sockaddr *)&client, &clen) < 0) {
		cout << "server: recvfrom failed: " << std::strerror(errno) << endl;
	}

	if (strncmp(buf, "CLIENT BYE", 10) != 0) {
		cout << "server: client bye failed" << endl;
		abort();
	}

	cout << "Client bye received" << endl;

	// Send server bye
	memset(buf, 0, BUFLEN);
	sprintf(buf, "SERVER BYE");
	if (sendto(fd, buf, BUFLEN, 0, (const struct sockaddr *)&client, addrlen) == -1) {
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
