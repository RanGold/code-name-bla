/* Program limits */
#define MAX_PACKET_SIZE 2048
#define DEAFULT_PORT 6900
#define MAX_FILE_NAME 1024
#define MAX_ERROR_MESSAGE 1024
#define MAX_MODE 20
#define MAX_DATA_BLOCK_SIZE 512
#define MODE_OCTET "octet"

/* Request configuration */
#define MAX_RETRIES 3
#define TIMEOUT_USEC 100000
#define PACKET_READY 1
#define PACKET_NOT_READY 0

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
#include <sys/select.h>
#include <netdb.h>

typedef struct {
	short dataSize;
	short opCode;
	char fileName[MAX_FILE_NAME];
	char mode[MAX_MODE];
	unsigned short blockNumber;
	unsigned char data[MAX_DATA_BLOCK_SIZE];
	short errorCode;
	char errorMessege[MAX_ERROR_MESSAGE];
} TFTPPacket;

typedef struct {
	unsigned int addrLen;
	struct sockaddr_storage clientAddr;
	FILE *file;
	int clientSocket;
} ClientData;

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

void clear_clientData(ClientData *clientData) {
	memset(clientData, 0, sizeof(ClientData));
	/* TODO : add additional ops */
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

int recv_packet(int listenSocket, ClientData *clientData, TFTPPacket *packet) {
	int res;
	unsigned char buffer[MAX_PACKET_SIZE];

	/* Receiving the next message */
	clientData->addrLen = sizeof(struct sockaddr_storage);
	memset(&(clientData->clientAddr), 0, clientData->addrLen);
	res = recvfrom(listenSocket, buffer, MAX_PACKET_SIZE, 0,
			(struct sockaddr*) &(clientData->clientAddr), &(clientData->addrLen));
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

	return (sizeof(short) + errorMsgLen + 1);
}

int send_packet(ClientData *clientData, TFTPPacket *packet) {
	unsigned char buffer[MAX_PACKET_SIZE];
	int messageSize = 2, res = 0;

	memset(buffer, 0, sizeof(buffer));
	insert_short_to_buffer(buffer, packet->opCode);

	/* Handling only the valid options */
	switch (packet->opCode) {
	case OP_ACK:
		messageSize += prepare_ack_packet(buffer + sizeof(short), packet);
		break;
	case OP_DATA:
		messageSize += prepare_data_packet(buffer + sizeof(short), packet);
		break;
	case OP_ERR:
		messageSize += prepare_err_packet(buffer + sizeof(short), packet);
		break;
	default:
		res = ERROR_LOGICAL;
	}

	if (res != 0) {
		return (res);
	}

	res = sendto(clientData->clientSocket, buffer, messageSize, 0, (struct sockaddr*)(&(clientData->clientAddr)), clientData->addrLen);
	return (res == -1 ? res : (res != messageSize ? ERROR_LOGICAL : 0));
}

int send_general_error(ClientData *clientData, int errorCode) {
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

	return (send_packet(clientData, &packet));
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
		if (absPath != NULL){
			free(absPath);
		}
		return (NULL);
	}

	/* Return file and free resources */
	if (absPath != NULL) {
		free(absPath);
	}
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

int wait_for_packet(int clientSocket) {
	fd_set readfds, errorfds;
	int res = 0;
	struct timeval tv;

	/* Initializing sets */
	FD_ZERO(&readfds);
	FD_ZERO(&errorfds);
	FD_SET(clientSocket, &readfds);
	FD_SET(clientSocket, &errorfds);

	tv.tv_sec = 0;
	tv.tv_usec = TIMEOUT_USEC;

	/* Waiting for incoming packet */
	res = select(clientSocket + 1, &readfds, NULL, &errorfds, &tv);
	if (res == -1) {
		return (ERROR);
	}

	if (FD_ISSET(clientSocket, &errorfds)) {
		return (ERROR);
	}

	if (FD_ISSET(clientSocket, &readfds)) {
		res = PACKET_READY;
	} else {
		res = PACKET_NOT_READY;
	}

	return (res);
}

int compare_sockaddr(struct sockaddr_storage *ss1, struct sockaddr_storage *ss2) {
	struct sockaddr_in *si1 = (struct sockaddr_in*) ss1;
	struct sockaddr_in *si2 = (struct sockaddr_in*) ss2;

	/* Comparing address and port */
	return ((memcmp(&(si1->sin_addr), &(si2->sin_addr), sizeof(struct in_addr)) == 0) &&
			(memcmp(&(si1->sin_port), &(si2->sin_port), sizeof(in_port_t)) == 0));
}

int handle_RRQ(ClientData *clientData, char *fileName) {
	TFTPPacket sendPacket, recvPacket;
	int res, retries;
	unsigned short curBlockNumber = 1;
	ClientData curSender;

	/* Checking if file exists */
	clientData->file = get_valid_file(fileName, "r");
	if (clientData->file  == NULL) {
		send_general_error(clientData, EC_FILE_NOT_FOUND);
		return (ERROR_LOGICAL);
	}

	/* Preparing first packet to send */
	clear_packet(&sendPacket);
	sendPacket.opCode = OP_DATA;
	sendPacket.blockNumber = curBlockNumber;
	sendPacket.dataSize = fread(sendPacket.data, 1, MAX_DATA_BLOCK_SIZE, clientData->file);
	if (ferror(clientData->file)) {
		fclose(clientData->file);
		send_general_error(clientData, get_EC_from_errno());
		return (ERROR);
	}

	res = send_packet(clientData, &sendPacket);
	if (res!= 0) {
		fclose(clientData->file);
		return (res);
	}

	retries = 0;

	/* waiting for client's ACK's */
	while (retries < MAX_RETRIES) {
		res = wait_for_packet(clientData->clientSocket);

		/* select's timeout reached */
		if (res == PACKET_NOT_READY) {
			retries++;
		}
		else if (res == ERROR){
			fclose(clientData->file);
			return (ERROR);
		}
		/* packet ready - get it */
		else if (res == PACKET_READY) {
			clear_clientData(&curSender);
			res = recv_packet(clientData->clientSocket, &curSender, &recvPacket);

			/* Checking packet validity */
			if (res != 0) {
				handle_return_value(send_general_error(clientData, EC_ILLEGAL_OPERATION));
				retries++;
			}
			/* Checking if the data was received from the original client */
			else if (!compare_sockaddr(&(clientData->clientAddr), &(curSender.clientAddr))) {
				handle_return_value(send_general_error(clientData, EC_UNKNOWN_TID));
				retries++;
			}
			/* Checking right kind of packet was received - only ACK allowed */
			else if (recvPacket.opCode != OP_ACK) {
				handle_return_value(send_general_error(clientData, EC_ILLEGAL_OPERATION));
				retries++;
			}
			/* Checking if ACK received on the correct block */
			else if (recvPacket.blockNumber != curBlockNumber) {
				/* Re-sending last packet */
				res = send_packet(clientData, &sendPacket);
				if (res != 0) {
					fclose(clientData->file);
					return (res);
				}

				retries++;
			}
			/* The packet is correct - send next block */
			else {
				retries = 0;
				curBlockNumber++;

				sendPacket.blockNumber = curBlockNumber;
				sendPacket.dataSize = fread(sendPacket.data, 1, MAX_DATA_BLOCK_SIZE, clientData->file);
				if (ferror(clientData->file)) {
					fclose(clientData->file);
					send_general_error(clientData, get_EC_from_errno());
					return (ERROR);
				}

				res = send_packet(clientData, &sendPacket);
				if (res != 0) {
					fclose(clientData->file);
					return (res);
				}

				if (feof(clientData->file)) {
					fclose(clientData->file);
					break;
				}
			}
		}
	}

	/* Checking if stopped due to max retries */
	if (retries >= MAX_RETRIES) {
		fclose(clientData->file);
		return (ERROR_LOGICAL);
	}

	return (res);
}

void WRQ_dallying(ClientData *clientData, TFTPPacket *ackPacket) {
	int res;
	ClientData curSender;
	TFTPPacket packet;

	res = wait_for_packet(clientData->clientSocket);
	if (res == PACKET_READY) {
		clear_clientData(&curSender);
		res = recv_packet(clientData->clientSocket, &curSender, &packet);

		/* Checking packet validity */
		if (res != 0) {
			handle_return_value(send_general_error(clientData, EC_ILLEGAL_OPERATION));
		}
		/* Checking if the data was received from the original client */
		else if (!compare_sockaddr(&(clientData->clientAddr), &(curSender.clientAddr))) {
			handle_return_value(send_general_error(clientData, EC_UNKNOWN_TID));
		}
		/* Checking right kind of packet was received */
		else if (packet.opCode != OP_DATA) {
			handle_return_value(send_general_error(clientData, EC_ILLEGAL_OPERATION));
		}
		/* Got data packet - resending last ACK */
		else {
			handle_return_value(send_packet(clientData, &packet));
		}
	}
}

int handle_WRQ(ClientData *clientData, char *fileName) {
	TFTPPacket packet;
	int res;
	unsigned short curBlockNumber = 0;
	int retries;
	ClientData curSender;
	int dataSize;

	/* Checking if file exists */
	clientData->file = get_valid_file(fileName, "r");
	if (clientData->file != NULL) {
		fclose(clientData->file);
		send_general_error(clientData, EC_FILE_EXISTS);
		return (ERROR_LOGICAL);
	}

	clientData->file = get_valid_file(fileName, "w");
	/* Checking if an error has occurred and getting correct packet to send */
	if (clientData->file == NULL) {
		send_general_error(clientData, get_EC_from_errno());
		return (ERROR);
	}

	/* Preparing first packet to send */
	clear_packet(&packet);
	packet.opCode = OP_ACK;
	packet.blockNumber = curBlockNumber;
	res = send_packet(clientData, &packet);
	if (res != 0) {
		fclose(clientData->file);
		return (res);
	}

	/* Waiting for packet */
	retries = 0;
	while (retries < MAX_RETRIES) {
		res = wait_for_packet(clientData->clientSocket);
		if (res == PACKET_NOT_READY) {
			retries++;
		} else if (res == ERROR) {
			fclose(clientData->file);

			/* Attempting deleting partial file */
			handle_return_value(remove(fileName) == 0 ? 0 : -1);
			return (ERROR);
		} else if (res == PACKET_READY) {
			clear_clientData(&curSender);
			res = recv_packet(clientData->clientSocket, &curSender, &packet);

			/* Checking packet validity */
			if (res != 0) {
				handle_return_value(send_general_error(clientData, EC_ILLEGAL_OPERATION));
				retries++;
			}
			/* Checking if the data was received from the original client */
			else if (!compare_sockaddr(&(clientData->clientAddr), &(curSender.clientAddr))) {
				handle_return_value(send_general_error(clientData, EC_UNKNOWN_TID));
				retries++;
			}
			/* Checking right kind of packet was received */
			else if (packet.opCode != OP_DATA) {
				handle_return_value(send_general_error(clientData, EC_ILLEGAL_OPERATION));
				retries++;
			}
			/* Checking if correct block was received */
			else if (packet.blockNumber != (unsigned short)(curBlockNumber + 1)) {
				/* Re-sending last ack */
				clear_packet(&packet);
				packet.opCode = OP_ACK;
				packet.blockNumber = curBlockNumber;
				res = send_packet(clientData, &packet);
				if (res != 0) {
					fclose(clientData->file);

					/* Attempting deleting partial file */
					handle_return_value(remove(fileName) == 0 ? 0 : -1);
					return (res);
				}

				retries++;
			}
			/* Got correct packet */
			else {
				retries = 0;
				curBlockNumber++;

				/* Writing data */
				res = fwrite(packet.data, 1, packet.dataSize, clientData->file);
				if (ferror(clientData->file)) {
					fclose(clientData->file);

					/* Attempting deleting partial file */
					handle_return_value(remove(fileName) == 0 ? 0 : -1);
					send_general_error(clientData, get_EC_from_errno());
					return (ERROR);
				} else if (res < packet.dataSize) {
					fclose(clientData->file);

					/* Attempting deleting partial file */
					handle_return_value(remove(fileName) == 0 ? 0 : -1);
					send_general_error(clientData, EC_NOT_DEFINED);
					return (ERROR);
				}

				dataSize = packet.dataSize;

				/* Sending ack */
				clear_packet(&packet);
				packet.opCode = OP_ACK;
				packet.blockNumber = curBlockNumber;
				res = send_packet(clientData, &packet);
				if (res != 0) {
					fclose(clientData->file);

					/* Attempting deleting partial file */
					handle_return_value(remove(fileName) == 0 ? 0 : -1);
					return (res);
				}

				/* EOF */
				if (dataSize < MAX_DATA_BLOCK_SIZE) {
					fclose(clientData->file);

					/* Trying to get additional packet, if the ack was lost */
					WRQ_dallying(clientData, &packet);
					break;
				}
			}
		}
	}

	/* Checking if stopped due to max retries */
	if (retries >= MAX_RETRIES) {
		fclose(clientData->file);

		/* Attempting deleting partial file */
		handle_return_value(remove(fileName) == 0 ? 0 : -1);
		return (ERROR_LOGICAL);
	}

	return (res);
}

int handle_request(ClientData *clientData, TFTPPacket *packet) {
	int res;

	if (packet->opCode == OP_RRQ || packet->opCode == OP_WRQ) {
		if (strcmp(packet->mode, MODE_OCTET) != 0) {
			send_general_error(clientData, EC_ILLEGAL_OPERATION);
			return (ERROR_LOGICAL);
		}
	}

	switch (packet->opCode) {
	case OP_RRQ:
		res = handle_RRQ(clientData, packet->fileName);
		break;
	case OP_WRQ:
		res = handle_WRQ(clientData, packet->fileName);
		break;
	default:
		res = send_general_error(clientData, EC_ILLEGAL_OPERATION);
		handle_return_value(res);
		res = ERROR_LOGICAL;
	}

	return (res);
}

int main(int argc, char** argv) {
	int res, listenSocket;
	ClientData clientData;
	TFTPPacket packet;

	listenSocket = initiallize_listen_socket();
	if (listenSocket == ERROR) {
		print_error();
		return (ERROR);
	}

	while (1) {
		clear_clientData(&clientData);

		/* Trying to receive an initial request */
		res = recv_packet(listenSocket, &clientData, &packet);
		res = handle_return_value(res);
		if (res == ERROR) {
			close(listenSocket);
			return (ERROR);
		}

		/* Get a socket to send with */
		clientData.clientSocket = socket(PF_INET, SOCK_DGRAM, 0);
		handle_return_value(clientData.clientSocket);
		if (clientData.clientSocket == -1) {
			continue;
		}

		/* Checking if a valid request was made and handling its process initialization */
		res = handle_request(&clientData, &packet);
		if (res != 0) {
			handle_return_value(res);
		}

		close(clientData.clientSocket);
	}

	/* Releasing resources */
	close(listenSocket);

	return (0);
}
