#include "common.h"

void print_error() {
	fprintf(stderr, "Error: %s\n", strerror(errno));
}

void print_error_message(char* message) {
	fprintf(stderr, "Error: %s\n", message);
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
		if (n == -1 || n == 0) {
			break;
		}
		total += n;
		bytesleft -= n;
	}

	*len = total; /* Return number actually sent here */

	return (n == -1 ? -1 : (n == 0 ? SOCKET_CLOSED : 0)); /* return -1 on failure, 0 on success */
}

int send_message(int targetSocket, Message *message, unsigned int *len) {
	int bytesToSend;
	int res;
	unsigned char* header;

	*len = 0;

	/* Sending header */
	bytesToSend = sizeof(int) + 1;
	header = calloc(bytesToSend, 1);
	if (header == NULL) {
		return (-1);
	}
	memcpy(header, (&(message->messageType)), 1);
	memcpy(header + 1, (&(message->dataSize)), sizeof(int));
	res = send_all(targetSocket, header, &bytesToSend);
	free(header);

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
	unsigned char* header;

	*len = 0;

	/* Receiving header */
	bytesToRecv = sizeof(int) + 1;
	header = calloc(bytesToRecv, 1);
	if (header == NULL) {
		return (-1);
	}
	res = recv_all(sourceSocket, header, &bytesToRecv);
	message->messageType = 0;
	memcpy((&(message->messageType)), header, 1);
	memcpy((&(message->dataSize)), header + 1, sizeof(int));
	free(header);

	/* In case of an error we stop before sending the data */
	if (res != 0) {
		return res;
	} else {
		*len = bytesToRecv;
	}

	/* Receiving data */
	bytesToRecv = message->dataSize;
	message->data = calloc(message->dataSize, 1);
	res = recv_all(sourceSocket, message->data, &bytesToRecv);
	if (res != 0) {
		free(message->data);
		return res;
	} else {
		*len += bytesToRecv;
	}

	return 0;
}

int prepare_message_from_string(char* str, Message* message) {
	message->messageType = String;
	message->dataSize = strlen(str);

	message->data = calloc(message->dataSize, 1);
	if (message->data != NULL) {
		memcpy(message->data, str, message->dataSize);
	}

	return (message->data == NULL ? -1 : 0);
}

int prepare_string_from_message(char** str, Message* message) {
	
	*str = (char*)calloc(message->dataSize, 1);
	strcpy(*str, (char *)message->data);
	free(message->data);
	return (*str == NULL ? -1 : 0);
}

void prepare_message_from_credentials(char* credentials, char *userName, char *password, Message *message) {

	credentials[0] = 0;
	sprintf(credentials, "%s\t%s", userName, password);

	message->messageType = Credentials;
	message->dataSize = strlen(credentials);
	message->data = (unsigned char*)credentials;
}

int send_empty_message(int socket, MessageType type) {

	Message message;
	unsigned int len;

	message.messageType = type;
	message.dataSize = 0;

	return (send_message(socket, &message, &len));
}

void free_mail_struct(Mail* mail) {

	int i;
	for (i = 0; i < mail->numAttachments; i++) {
		free(mail->attachments[i].data);
		free(mail->attachments[i].fileName);
	}
	free(mail->attachments);
	free(mail->body);
	free(mail->sender);
	free(mail->subject);
}

int send_message_from_mail(Mail* mail) {
	return 0;
}
