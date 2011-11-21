#define MAX_HOST_NAME_LEN 256
#define MAX_PORT_LEN 10

#include "common.h"

int main(int argc, char** argv) {

	/* Variables declaration */
	short port = 6423;
	char hostname[MAX_HOST_NAME_LEN] = "localhost", portString[10];
	struct in_addr tempServerAddr;
	int clientSocket;
	struct sockaddr_in serverAddr;
	int isIP;
	struct addrinfo hints, *servinfo;
	int rv, res;
	Message message;
	unsigned int len;

	/* Validate number of arguments */
	if (argc != 1 && argc != 2 && argc != 3) {
		fprintf(stderr, "Error: Usage mail_client "
			"[hostname [port]]\n");
		return (-1);
	}

	if (argc == 2) {
		if ((inet_pton(AF_INET, argv[1], &serverAddr) == 0) && (atoi(argv[1])
				!= 0)) {
			fprintf(stderr, "Error: Usage mail_client "
				"[hostname [port]]\n");
			return (-1);
		} else {
			strcpy(hostname, argv[1]);
		}
	}

	isIP = inet_pton(AF_INET, hostname, &tempServerAddr);
	if (argc == 3) {
		if (((isIP == 0) && (atoi(argv[1]) != 0)) || (atoi(argv[2]) == 0)) {
			fprintf(stderr, "Error: Usage mail_client "
				"[hostname [port]]\n");
			return (-1);
		} else {
			strcpy(hostname, argv[1]);
			port = atoi(argv[2]);
		}
	}

	clientSocket = socket(PF_INET, SOCK_STREAM, 0);

	sprintf(portString, "%d", port);
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	rv = getaddrinfo(argv[1], portString, &hints, &servinfo);

	if (rv != 0) {
		fprintf(stderr, "Error: Usage mail_client "
			"[hostname [port]]\n");
		freeaddrinfo(servinfo);
		return (-1);
	}

	/* connect to server */
	res = connect(clientSocket, servinfo->ai_addr, servinfo->ai_addrlen);
	if (res == -1) {
		print_error();
		freeaddrinfo(servinfo);
		return (-1);
	}

	/* Receiving welcome message */
	res = recv_message(clientSocket, &message, &len);

	if (res == -1 || message.messageType != string) {
		/* TODO : free stuff */
		print_error();
	} else {
		printf((const char*)((void *)message.data));
	}

	/*
	 CHAR buf[256];
	 ZeroMemory(buf,sizeof(buf));

	 int nBytesToSend=sizeof(buf);
	 int iPos=0;
	 */

	/*
	//prepare buffer for incoming data
	int nLeft = sizeof(buf);

	// PROBLEM - While data is recieved no listen is happening other connection fails
	do //loop till there are no more data
	{
		int nNumBytes = recv(hClientSocket, buf + iPos, nLeft, 0);

		//check if cleint closed connection
		if (!nNumBytes)
			break;

		assert(nNumBytes != SOCKET_ERROR);

		//update free space and pointer to next byte
		nLeft -= nNumBytes;
		iPos += nNumBytes;

	} while (1);
*/
	/* close connection and socket */
	close(clientSocket);

	return 0;
}

