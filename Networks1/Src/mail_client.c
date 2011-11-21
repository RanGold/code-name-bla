#define MAX_HOST_NAME_LEN 256
#define MAX_PORT_LEN 7
#define DEFAULT_HOST_NAME "localhost"
#define DEFAULT_PORT "6423"
#define CLIENT_USAGE_MSG "Error: Usage mail_client [hostname [port]]\n"

#include "common.h"

int main(int argc, char** argv) {

	/* Variables declaration */
	char hostname[MAX_HOST_NAME_LEN + 1] = DEFAULT_HOST_NAME;
	char portString[MAX_PORT_LEN + 1] = DEFAULT_PORT;
	int clientSocket;
	struct addrinfo hints, *servinfo;
	int res;
	Message message;
	unsigned int len;
	char* stringMessage;

	/* Validate number of arguments */
	if (argc != 1 && argc != 2 && argc != 3) {
		fprintf(stderr, CLIENT_USAGE_MSG);
		return (-1);
	} else if (argc == 2) {
		strncpy(hostname, argv[1], MAX_HOST_NAME_LEN);
	} else if (argc == 3) {
		strncpy(hostname, argv[1], MAX_HOST_NAME_LEN);
		strncpy(portString, argv[2], MAX_PORT_LEN);
	}

	/* Checking for address validity */
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	res = getaddrinfo(hostname, portString, &hints, &servinfo);
	if (res != 0) {
		fprintf(stderr, "%s\n", gai_strerror(res));
		fprintf(stderr, CLIENT_USAGE_MSG);
		return (-1);
	}

	clientSocket = socket(PF_INET, SOCK_STREAM, 0);

	/* Connect to server */
	res = connect(clientSocket, servinfo->ai_addr, servinfo->ai_addrlen);
	if (res == -1) {
		print_error();
		fprintf(stderr, CLIENT_USAGE_MSG);
		freeaddrinfo(servinfo);
		return (-1);
	}

	/* Receiving welcome message */
	res = recv_message(clientSocket, &message, &len);

	if (res == -1 || message.messageType != string) {
		close(clientSocket);
		freeaddrinfo(servinfo);
		if (res == -1) {
			print_error();
		}
		return (-1);
	} else {
		prepare_string_from_message(&stringMessage, &message);
		printf("%s\n", stringMessage);
		free(message.data);
	}

	/* Close connection and socket */
	close(clientSocket);
	freeaddrinfo(servinfo);

	return (0);
}

