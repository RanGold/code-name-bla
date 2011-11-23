/* system parameters */
#define MAX_HOST_NAME_LEN 256
#define MAX_INPUT_LEN 256
#define MAX_PORT_LEN 7
#define DEFAULT_HOST_NAME "localhost"
#define DEFAULT_PORT "6423"

/* errors messages definitions */
#define CLIENT_USAGE_MESSAGE "Usage mail_client [hostname [port]]"
#define CREDENTIALS_USAGE_MESSAGE "Expected:\nUser: [username]\nPassword: [password]"
#define WRONG_CREDENTIALS_MESSAGE "Wrong credentials"

/* commands definitions */
#define QUIT_MESSAGE "QUIT"
#define SHOW_INBOX "SHOW_INBOX"

/* general definitions */
#define CONNECTION_SUCCEED "Connected to server"



#include "common.h"

/* return -1 on error
 *         1 on accept
 *		   0 on reject */
int recv_credentials_result (int sourceSocket, Message *message, unsigned int *len){
	int res;

	res = recv_message(sourceSocket, message, len);
	if (res == -1){
		return res;
	} else if (message->messageType == CredentialsAccept){
		return 1;
	} else if (message->messageType == CredentialsDeny){
		return 0;
	} else {
		return ERROR;
	}
}

int send_credentials(char* userName, char* password,  int sourceSocket, Message *message, unsigned int *len){
	char credentials[MAX_NAME_LEN + MAX_PASSWORD_LEN + 2];

	prepare_message_from_credentials(credentials, userName, password, message);
	return (send_message(sourceSocket, message, len));
}

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
	char userName[MAX_NAME_LEN + 1];
	char password[MAX_PASSWORD_LEN + 1];

	char input[MAX_INPUT_LEN + 1];
	int isLoggedIn = 0;

	/* Validate number of arguments */
	if (argc != 1 && argc != 2 && argc != 3) {
		print_error_message(CLIENT_USAGE_MESSAGE);
		return ERROR ;
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
		print_error_message(CLIENT_USAGE_MESSAGE);
		return ERROR;
	}

	clientSocket = socket(PF_INET, SOCK_STREAM, 0);

	/* Connect to server */
	res = connect(clientSocket, servinfo->ai_addr, servinfo->ai_addrlen);
	if (res == ERROR) {
		print_error();
		freeaddrinfo(servinfo);
		return (ERROR);
	}

	/* Receiving welcome message */
	res = recv_message(clientSocket, &message, &len);

	if (res == ERROR || message.messageType != String) {
		close(clientSocket);
		freeaddrinfo(servinfo);
		if (res == ERROR) {
			print_error();
		}
		return ERROR;
	} else {
		prepare_string_from_message(&stringMessage, &message);
		printf("%s\n", stringMessage);
		free(stringMessage);
	}

	do {
		fgets(input, MAX_INPUT_LEN, stdin);
		if (strcmp(input, QUIT_MESSAGE) == 0) {
			if (send_empty_message(clientSocket, Quit) == 0) {
				break;
			} else {
				print_error();
				break;
			}
		} else if (!isLoggedIn) {
			if ((sscanf(input, "User: %s", userName) +
					scanf("Password: %s", password)) != 2) {
				print_error_message(CREDENTIALS_USAGE_MESSAGE);
			} else {
				res = send_credentials(userName, password, clientSocket, &message, &len);

				res = recv_credentials_result(clientSocket, &message, &len);

				if (res == 1){     /*TODO: need to check for ERROR */
					isLoggedIn = 1;
					printf("Connected to server");
				} else if (res == 0){
					print_error_message(WRONG_CREDENTIALS_MESSAGE);
				}
			}
			fgets(input, MAX_INPUT_LEN, stdin); /*flushing*/
		} else {  /* user is logged in */
			if (strcmp(input,SHOW_INBOX) == 0){

			}

		}

	} while (1);

	/* Close connection and socket */
	/*TODO: maybe free Message data ?*/
	close(clientSocket);
	freeaddrinfo(servinfo);

	return (0);
}

