#define MAX_PACKET_SIZE 2048
#define DEAFULT_PORT 6900
#define MAX_WELL_KNOWN_PORT 1023
#define MAX_FILE_NAME 1024
#define MAX_ERROR_MESSAGE 1024
#define MAX_MODE 20
#define MAX_DATA_BLOCK_SIZE 512
#define MODE_OCTET "octet"
#define MAX_RETRIES 3
#define TIMEOUT_USEC 100000

/* Errors */
#define ERROR -1
#define ERROR_SOCKET_CLOSED -2
#define ERROR_LOGICAL -3

/* Errors Messages */
#define SOCKET_CLOSED_MESSAGE "Socket closed"
#define INVALID_DATA_MESSAGE "Invalid data received"

/* TFTP OP Codes */
#define OP_RRQ 1
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERR 5

/* TFTP error codes and message */
#define EC_NOT_DEFINED 0
#define EC_MSG_NOT_DEFINED "Undefined error has occurred"
#define EC_FILE_NOT_FOUND 1
#define EC_MSG_FILE_NOT_FOUND "File not found"
#define EC_FILE_ACCESS_VIOLATION 2
#define EC_MSG_FILE_ACCESS_VIOLATION "Access violation"
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
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <wordexp.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

typedef struct {
	short dataSize;
	short opCode;
	char fileName[MAX_FILE_NAME];
	char mode[MAX_MODE];
	short blockNumber;
	unsigned char data[MAX_DATA_BLOCK_SIZE];
	short errorCode;
	char errorMessege[MAX_ERROR_MESSAGE];
} TFTPPacket;

void print_error() {
	fprintf(stderr, "Error: %s\n", strerror(errno));
}

void print_error_message(char* message) {
	fprintf(stderr, "Error: %s\n", message);
}

int handle_return_value(int res) {
	if (res == ERROR) {
		print_error();
	} else if (res == ERROR_LOGICAL) {
		print_error_message(INVALID_DATA_MESSAGE);
	} else if (res == ERROR_SOCKET_CLOSED) {
		print_error_message(SOCKET_CLOSED_MESSAGE);
	}

	return (res);
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

	return (listenSocket);
}

void clear_packet(TFTPPacket *packet) {
	memset(packet, 0, sizeof(TFTPPacket));
	packet->blockNumber = -1;
	packet->errorCode = -1;
	packet->opCode = -1;
	packet->dataSize = -1;
}

short get_short_from_buffer(unsigned char *buffer) {
	short netShort;

	memcpy(&netShort, buffer, sizeof(short));
	return (ntohs(netShort));
}

void insert_short_to_buffer(unsigned char *buffer, short num) {
	short netShort;

	netShort = htons(num);
	memcpy(buffer, &netShort, sizeof(short));
}

int parse_ack_packet(unsigned char *buffer, int bufferLen, TFTPPacket *packet) {
	if (bufferLen < sizeof(short)) {
		return (ERROR_LOGICAL);
	}

	/* Getting the block number */
	packet->blockNumber = get_short_from_buffer(buffer);

	return (0);
}

int parse_data_packet(unsigned char *buffer, int bufferLen, TFTPPacket *packet) {
	int res;

	res = parse_ack_packet(buffer, bufferLen, packet);
	if (res != 0) {
		return (res);
	}

	buffer += sizeof(short);
	bufferLen -= sizeof(short);

	/* Setting the packet's data */
	packet->dataSize = bufferLen;
	if (packet->dataSize > MAX_DATA_BLOCK_SIZE) {
		return (ERROR_LOGICAL);
	}
	memcpy(&(packet->data), buffer, bufferLen);

	return (res);
}

int parse_error_packet(unsigned char *buffer, int bufferLen, TFTPPacket *packet) {
	int res = 0;

	/* Same structure as the ack message prefix */
	res = parse_ack_packet(buffer, bufferLen, packet);
	if (res != 0) {
		return (res);
	}

	buffer += sizeof(short);
	bufferLen -= sizeof(short);

	/* Validating the packet's structure */
	if (buffer[bufferLen - 1] != 0) {
		return (ERROR_LOGICAL);
	}

	/* Setting the error message */
	memcpy(&(packet->errorMessege), buffer, bufferLen < MAX_ERROR_MESSAGE ? bufferLen : (MAX_ERROR_MESSAGE - 1));

	return (res);
}

int parse_request_packet(unsigned char *buffer, int bufferLen, TFTPPacket *packet) {
	int fileNameLength, modeLength, i;

	/* Getting file name */
	fileNameLength = strlen((char*)buffer);
	if (buffer[fileNameLength] != 0) {
		return (ERROR_LOGICAL);
	}
	memcpy(packet->fileName, buffer, fileNameLength < MAX_FILE_NAME ? fileNameLength : (MAX_FILE_NAME - 1));


	/* Getting mode */
	buffer += fileNameLength + 1;
	modeLength = strlen((char*)buffer);
	if (buffer[modeLength] != 0) {
		return (ERROR_LOGICAL);
	}
	memcpy(packet->mode, buffer, modeLength < MAX_MODE ? modeLength : (MAX_MODE - 1));
	for (i = 0; i < strlen(packet->mode); i++) {
		packet->mode[i] = tolower(packet->mode[i]);
	}

	return (0);
}

int parse_packet(unsigned char* buffer, int bufferLen, TFTPPacket *packet) {
	int res = 0;

	if (bufferLen < sizeof(short)) {
		return (ERROR_LOGICAL);
	}

	clear_packet(packet);

	/* Getting the opcode */
	packet->opCode = get_short_from_buffer(buffer);
	buffer += sizeof(short);
	bufferLen -= sizeof(short);

	switch (packet->opCode) {
	case OP_ACK:
		res = parse_ack_packet(buffer, bufferLen, packet);
		break;
	case OP_DATA:
		res = parse_data_packet(buffer, bufferLen, packet);
		break;
	case OP_ERR:
		res = parse_error_packet(buffer, bufferLen, packet);
		break;
	case OP_RRQ:
	case OP_WRQ:
		res = parse_request_packet(buffer, bufferLen, packet);
		break;
	default:
		res = ERROR_LOGICAL;
	}

	/* Clearing invalid packet */
	if (res != 0) {
		clear_packet(packet);
	}
	return (res);
}

int recv_packet(int listenSocket, struct sockaddr_storage* from, unsigned int *addrLen, TFTPPacket *packet) {
	int res;
	unsigned char buffer[MAX_PACKET_SIZE];

	/* Receiving the next message */
	memset(from, 0, sizeof(struct sockaddr_storage));
	*addrLen = sizeof(struct sockaddr_storage);
	res = recvfrom(listenSocket, buffer, MAX_PACKET_SIZE, 0,
			(struct sockaddr*) from, addrLen);
	if (res == -1) {
		return (ERROR);
	} else if (res == 0) {
		return (ERROR_SOCKET_CLOSED);
	}

	/* Parsing the message into a TFTP packet */
	res = parse_packet(buffer, res, packet);

	return (res);
}

int prepare_ack_packet(unsigned char *buffer, TFTPPacket *packet) {
	insert_short_to_buffer(buffer, packet->blockNumber);

	return (sizeof(short));
}

int prepare_data_packet(unsigned char *buffer, TFTPPacket *packet) {
	/* Inserting block number */
	prepare_ack_packet(buffer, packet);

	/* Inserting data */
	memcpy(buffer + sizeof(short), packet->data, packet->dataSize);

	return (sizeof(short) + packet->dataSize);
}

int prepare_err_packet(unsigned char *buffer, TFTPPacket *packet) {
	int errorMsgLen;

	/* Inserting error code */
	insert_short_to_buffer(buffer, packet->errorCode);

	/* Inserting error message */
	errorMsgLen = strlen(packet->errorMessege);
	memcpy(buffer + sizeof(short), packet->errorMessege, errorMsgLen);

	return (sizeof(short) + errorMsgLen + 2);
}

int send_packet(int fromSocket, struct sockaddr_storage *clientAddr, unsigned int addrLen, TFTPPacket *packet) {
	unsigned char buffer[MAX_PACKET_SIZE];
	int messageSize, res = 0;

	insert_short_to_buffer(buffer, packet->opCode);

	/* Handling only the valid options */
	switch (packet->opCode) {
	case OP_ACK:
		messageSize = prepare_ack_packet(buffer + sizeof(short), packet);
		break;
	case OP_DATA:
		messageSize = prepare_data_packet(buffer + sizeof(short), packet);
		break;
	case OP_ERR:
		messageSize = prepare_err_packet(buffer + sizeof(short), packet);
		break;
	default:
		res = ERROR_LOGICAL;
	}

	if (res != 0) {
		return (res);
	}

	res = sendto(fromSocket, buffer, messageSize, 0, (struct sockaddr*)clientAddr, addrLen);
	return (res == -1 ? res : (res != messageSize ? ERROR_LOGICAL : 0));
}

int send_general_error(int fromSocket, struct sockaddr_storage *clientAddr, unsigned int addrLen, int errorCode) {
	TFTPPacket packet;

	/* Preparing packet */
	clear_packet(&packet);
	packet.opCode = OP_ERR;
	packet.errorCode = errorCode;

	/* Preparing error message */
	switch (errorCode) {
	case EC_DISK_FULL:
		strcpy(packet.errorMessege, EC_MSG_DISK_FULL);
		break;
	case EC_FILE_ACCESS_VIOLATION:
		strcpy(packet.errorMessege, EC_MSG_FILE_ACCESS_VIOLATION);
		break;
	case EC_FILE_EXISTS:
		strcpy(packet.errorMessege, EC_MSG_FILE_EXISTS);
		break;
	case EC_FILE_NOT_FOUND:
		strcpy(packet.errorMessege, EC_MSG_FILE_NOT_FOUND);
		break;
	case EC_ILLEGAL_OPERATION:
		strcpy(packet.errorMessege, EC_MSG_ILLEGAL_OPERATION);
		break;
	case EC_NOT_DEFINED:
		strcpy(packet.errorMessege, EC_MSG_NOT_DEFINED);
		break;
	case EC_NO_SUCH_USER:
		strcpy(packet.errorMessege, EC_MSG_NO_SUCH_USER);
		break;
	case EC_UNKNOWN_TID:
		strcpy(packet.errorMessege, EC_MSG_UNKNOWN_TID);
		break;
	default:
		strcpy(packet.errorMessege, EC_MSG_NOT_DEFINED);
	}

	return (send_packet(fromSocket, clientAddr, addrLen, &packet));
}

int get_absolute_path(char* relPath, char** absPath) {

	wordexp_t expansionResult;
	int expLength, i;

	wordexp(relPath, &expansionResult, 0);
	if (expansionResult.we_wordc == 0) {
		return (ERROR);
	}
	expLength = 0;
	for (i = 0; i < expansionResult.we_wordc; i++) {
		expLength += strlen(expansionResult.we_wordv[i]);
	}
	*absPath = calloc(expLength + expansionResult.we_wordc, 1);
	if (*absPath == NULL) {
		return (ERROR);
	}

	strncpy(*absPath, expansionResult.we_wordv[0], strlen(expansionResult.we_wordv[0]));
	for (i = 1; i < expansionResult.we_wordc; i++) {
		strcat(*absPath, " ");
		strcat(*absPath, expansionResult.we_wordv[i]);
	}
	wordfree(&expansionResult);

	return (0);
}

FILE* get_valid_file(char* fileName, char* mode) {

	FILE* file;
	char* absPath = NULL;
	int res;

	/* Getting absolute path */
	res = get_absolute_path(fileName, &absPath);
	if (res == ERROR) {
		return (NULL);
	}

	/* Open, validate and return file */
	file = fopen(absPath, mode);
	if (file == NULL) {
		free(absPath);
		return (NULL);
	}

	/* Return file and free resources */
	free(absPath);
	return (file);
}

int get_EC_from_errno() {
	switch (errno) {
	case EACCES:
		return (EC_FILE_ACCESS_VIOLATION);
	case ENOENT:
		return (EC_FILE_NOT_FOUND);
	case EEXIST:
		return (EC_FILE_EXISTS);
	case ENOSPC:
		return (EC_DISK_FULL);
	default:
		return (EC_NOT_DEFINED);
	}
}

int handle_RRQ(int fromSocket, struct sockaddr_storage *clientAddr, unsigned int addrLen, TFTPPacket *packet) {
	TFTPPacket sendPacket;
	int res;
	FILE *file;

	if (strcmp(packet->mode, MODE_OCTET) != 0) {
		send_general_error(fromSocket, clientAddr, addrLen, EC_ILLEGAL_OPERATION);
		return (ERROR_LOGICAL);
	}

	file = get_valid_file(packet->fileName, "r");
	/* Checking if an error has occurred and getting correct packet to send */
	if (file == NULL) {
		send_general_error(fromSocket, clientAddr, addrLen, get_EC_from_errno());
		return (ERROR);
	}

	/* Preparing first packet to send */
	clear_packet(&sendPacket);
	sendPacket.opCode = OP_DATA;
	sendPacket.blockNumber = 1;
	fread(sendPacket.data, 1, MAX_DATA_BLOCK_SIZE, file);
	if (ferror(file)) {
		fclose(file);
		send_general_error(fromSocket, clientAddr, addrLen, get_EC_from_errno());
		return (ERROR);
	}
	fclose(file);

	res = send_packet(fromSocket, clientAddr, addrLen, &sendPacket);
	return (res);
}

int handle_WRQ(int fromSocket, struct sockaddr_storage *clientAddr, unsigned int addrLen, TFTPPacket *packet) {
	TFTPPacket sendPacket;
	int res;
	FILE *file;

	if (strcmp(packet->mode, MODE_OCTET) != 0) {
		send_general_error(fromSocket, clientAddr, addrLen, EC_ILLEGAL_OPERATION);
		return (ERROR_LOGICAL);
	}

	/* Checking if file exists */
	file = get_valid_file(packet->fileName, "r");
	if (file != NULL) {
		fclose(file);
		send_general_error(fromSocket, clientAddr, addrLen, EC_FILE_EXISTS);
		return (ERROR);
	}

	file = get_valid_file(packet->fileName, "w");
	/* Checking if an error has occurred and getting correct packet to send */
	if (file == NULL) {
		send_general_error(fromSocket, clientAddr, addrLen, get_EC_from_errno());
		return (ERROR);
	}
	fclose(file);

	/* Preparing first packet to send */
	clear_packet(&sendPacket);
	sendPacket.opCode = OP_ACK;
	sendPacket.blockNumber = 0;

	res = send_packet(fromSocket, clientAddr, addrLen, &sendPacket);
	return (res);
}

int handle_initial_request(int fromSocket, struct sockaddr_storage *clientAddr, unsigned int addrLen, TFTPPacket *packet) {
	int res;

	switch (packet->opCode) {
	case OP_RRQ:
		res = handle_RRQ(fromSocket, clientAddr, addrLen, packet);
		break;
	case OP_WRQ:
		res = handle_WRQ(fromSocket, clientAddr, addrLen, packet);
		break;
	default:
		res = send_general_error(fromSocket, clientAddr, addrLen, EC_ILLEGAL_OPERATION);
		handle_return_value(res);
		res = ERROR_LOGICAL;
	}

	return (res);
}

int wait_for_packet(int socket) {
	/* TODO: use select for trying to get a packet */
}

int get_block_from_file(char* fileName, int blockNumber, unsigned char *buffer, unsigned int *bufferSize) {
	/* TODO: */
}

int main(int argc, char** argv) {
	int res, listenSocket;
	int curConnetionSocket;
	unsigned int addrLen;
	struct sockaddr_storage clientAddr;
	TFTPPacket packet;
	char curFileName[MAX_FILE_NAME];
	int isWrite;
	int curBlock;

	listenSocket = initiallize_listen_socket();
	if (listenSocket == ERROR) {
		print_error();
		return (ERROR);
	}

	while (1) {
		/* Trying to receive an initial request */
		res = recv_packet(listenSocket, &clientAddr, &addrLen, &packet);
		res = handle_return_value(res);
		if (res == ERROR) {
			close(listenSocket);
			return (ERROR);
		}

		/* Get a socket to send with */
		curConnetionSocket = socket(PF_INET, SOCK_DGRAM, 0);
		handle_return_value(curConnetionSocket);
		if (curConnetionSocket == -1) {
			continue;
		}

		/* Handling general error on initial request */
		if (res != 0) {
			res = send_general_error(curConnetionSocket, &clientAddr, addrLen, EC_NOT_DEFINED);
			handle_return_value(res);
			continue;
		}

		/* Checking if a valid request was made and handling its process initialization */
		res = handle_initial_request(curConnetionSocket, &clientAddr, addrLen, &packet);
		if (res != 0) {
			handle_return_value(res);
		} else {
			/* Preparing for client interaction */
			isWrite = packet.opCode == OP_WRQ;
			strcpy(curFileName, packet.fileName);
			curBlock = isWrite ? 0 : 1;

			/* Handling a client */
			while (1) {
				if (isWrite) {

				} else {

				}
			}
		}

		close(curConnetionSocket);
		curConnetionSocket = -1;
	}

	/* Releasing resources */
	close(listenSocket);

	return (0);
}
