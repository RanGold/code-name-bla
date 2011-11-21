#include "common.h"

void print_error() {
	printf("error: %s", strerror(errno));
}

/* Source: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
int send_all(int targetSocket, unsigned char *buf, int *len) {
	int total = 0; /* How many bytes we've sent */
	int bytesleft = *len; /* How many we have left to send */
	int n;

	while (total < *len) {
		n = send(targetSocket, buf + total, bytesleft, 0);
		if (n == -1) {
			break;
		}
		total += n;
		bytesleft -= n;
	}

	*len = total; /* Return number actually sent here */

	return n == -1 ? -1 : 0; /* return -1 on failure, 0 on success */
}

int recv_all(int sourceSocket, unsigned char *buf, int *len) {
	int total = 0; /* How many bytes we've sent */
	int bytesleft = *len; /* How many we have left to send */
	int n;

	while (total < *len) {
		n = recv(sourceSocket, buf + total, bytesleft, 0);
		if (n == -1) {
			break;
		}
		total += n;
		bytesleft -= n;
	}

	*len = total; /* Return number actually sent here */

	return n == -1 ? -1 : 0; /* return -1 on failure, 0 on success */
}

int send_message(int targetSocket, Message *message, unsigned int *len) {
	int bytesToSend;
	int res;

	*len = 0;

	/* Sending header */
	bytesToSend = sizeof(int) * 2;
	res = send_all(targetSocket, (void*) message, &bytesToSend);

	/* In case of an error we stop before sending the data */
	if (res == -1) {
		return -1;
	} else {
		*len = bytesToSend;
	}

	/* Sending data */
	bytesToSend = message->dataSize;
	res = send_all(targetSocket, message->data, &bytesToSend);
	if (res == -1) {
		return -1;
	} else {
		*len += bytesToSend;
	}

	return 0;
}

int recv_message(int sourceSocket, Message *message, unsigned int *len) {
	int bytesToRecv;
	int res;

	*len = 0;

	/* Receiving header */
	bytesToRecv = sizeof(int) * 2;
	res = recv_all(sourceSocket, (void*) message, &bytesToRecv);

	/* In case of an error we stop before sending the data */
	if (res == -1) {
		return -1;
	} else {
		*len = bytesToRecv;
	}

	/* Receiving data */
	bytesToRecv = message->dataSize;
	message->data = calloc(message->dataSize, 1);
	res = recv_all(sourceSocket, message->data, &bytesToRecv);
	if (res == -1) {
		free(message->data);
		return -1;
	} else {
		*len += bytesToRecv;
	}

	return 0;
}

int prepare_message_from_string (char* str, Message* message) {
	message->messageType = string;
	message->dataSize = strlen(str) + 1;

	message->data = calloc(message->dataSize, 1);
	if (message->data != NULL) {
		memcpy(message->data, str, message->dataSize);
	}

	return (message->data == NULL ? -1 : 0);
}

int prepare_string_from_message (char** str, Message* message) {

	*str = (char*)message->data;

	return 0;
}

