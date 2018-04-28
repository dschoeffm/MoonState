#include <cerrno>
#include <iostream>
#include <thread>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

volatile bool stopFlag = false;

void signalHandler(int sig) {
	(void)sig;
	stopFlag = true;
}

void runConnect(struct sockaddr_in server) {
	while (!stopFlag) {
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) {
			std::cout << "socket() failed" << std::endl;
			return;
		}

		if (connect(fd, reinterpret_cast<sockaddr *>(&server), sizeof(server)) < 0) {
			std::cout << strerror(errno) << std::endl;
			std::cout << "connect() failed" << std::endl;
			return;
		}

		close(fd);
	}
}

int main(int argc, char **argv) {
	struct sockaddr_in serveraddr[256];

	if (argc != 4) {
		std::cout << "Usage: " << argv[0] << " <ip> <start-port> <# threads>" << std::endl;
		return 1;
	}
	uint16_t startPort = atoi(argv[2]);

	int numThreads = atoi(argv[3]);
	if (numThreads > 256) {
		std::cout << "A maximum of 256 threads are allowed" << std::endl;
		return 1;
	}

	memset(serveraddr, 0, sizeof(struct sockaddr_in) * 256);

	if (inet_pton(AF_INET, argv[1], &((serveraddr[0]).sin_addr)) != 1) {
		std::cout << "Please provide a valid IP address" << std::endl;
		return 1;
	}

	serveraddr[0].sin_family = AF_INET;
	serveraddr[0].sin_port = htons(startPort);

	for (int i = 1; i < numThreads; i++) {
		serveraddr[i].sin_family = AF_INET;
		serveraddr[i].sin_addr.s_addr = serveraddr[0].sin_addr.s_addr;
		serveraddr[i].sin_port = htons(startPort + i);
	}

	std::thread connectThreads[256];

	for (int i = 0; i < numThreads; i++) {
		connectThreads[i] = std::thread(runConnect, serveraddr[i]);
	}

	for (int i = 0; i < numThreads; i++) {
		connectThreads[i].join();
	}

	return 0;
}
