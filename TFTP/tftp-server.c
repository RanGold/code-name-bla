#define MAX_PACKET_SIZE 2048
#define DEAFULT_PORT 6900
#define MAX_WELL_KNOWN_PORT 1023
#define ERROR -1

/* TFTP OP Codes */
#define OP_RRQ 1
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERR 5

/* TFTP error codes and message */
#define EC_NOT_DEFINED 0
#define EC_FILE_NOT_FOUND 1
#define EC_MSG_FILE_NOT_FOUND "File not found"
#define EC_FILE_ACCESS_VIOLATION 2
#define EC_MSG_ACCESS_VIOLATION "Access violation"
#define EC_DISK_FULL 3
#define EC_MSG_DISK_FULL "Disk full or allocation exceeded"
#define EC_ILLEGAL_OPERATION 4
#define EC_MSG_ILLEGAL_OPERATION "Illegal TFTP operation"
#define EC_UNKNOWN_TID 5
#define EC_MSG_UNKNOWN_TID "Unknown transfer ID"
#define EC_FILE_EXISTS 6
#define EC_MSG_FILE_EXISTS "File already exists"
#define EC_NO_SUCH_USER 7
#define EC_MSG_NO_SUCH_USER "No such user"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

void print_error() {
	fprintf(stderr, "Error: %s\n", strerror(errno));
}

void print_error_message(char* message) {
	fprintf(stderr, "Error: %s\n", message);
}

int initiallize_listen_socket() {
	struct sockaddr_in serverAddr;
	int res, listenSocket;

	/* Create listen socket - needs to be same as the client */
	listenSocket = socket(PF_INET, SOCK_DGRAM, 0);
	if (listenSocket == -1) {
		return (ERROR);
	}

	/* Prepare address structure to bind listen socket */
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddr.sin_port = htons(DEAFULT_PORT);

	/* Bind listen socket - no other processes can bind to this port */
	res = bind(listenSocket, (struct sockaddr*) &serverAddr,
			sizeof(serverAddr));
	if (res == -1) {
		close(listenSocket);
		return (ERROR);
	}

	return listenSocket;
}

int recv_packet(int listenSocket, struct sockaddr_storage* from) {
	int res;
	unsigned int addrLen;
	unsigned char buffer[MAX_PACKET_SIZE];

	memset(from, 0, sizeof(struct sockaddr_storage));
	addrLen = sizeof(struct sockaddr_storage);
	res = recvfrom(listenSocket, buffer, MAX_PACKET_SIZE, 0,
			(struct sockaddr*) from, &addrLen);
	if (res == -1) {
		return (ERROR);
	}

	return (0);
}

int main(int argc, char** argv) {
	int res, listenSocket;
	int curConnetionPort;
	struct sockaddr_storage clientAddr;

	listenSocket = initiallize_listen_socket();
	if (listenSocket == ERROR) {
		print_error();
		return (ERROR);
	}

	/* Initialize random seed */
	srand(time(NULL));

	while(1) {
		res = recv_packet(listenSocket, &clientAddr);
		if (res == ERROR) {
			close(listenSocket);
			print_error();
			return (ERROR);
		}

		/* Generate a random TID */
		curConnetionPort = (rand() % (UINT16_MAX - MAX_WELL_KNOWN_PORT)) + MAX_WELL_KNOWN_PORT + 1;
	}

	/* Releasing resources */
	close(listenSocket);

	return (0);
}
