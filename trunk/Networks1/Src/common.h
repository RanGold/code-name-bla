#define MAX_NAME_LEN 50
#define MAX_PASSWORD_LEN 50
#define MAX_EMAILS 32000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

typedef enum MessageType {
	string,
	mail
} MessageType;

typedef struct Attachment {
	char* fileName;
	int size;
	unsigned char* data;
} Attachment;

typedef struct Mail {
	int id;
	char* sender;
	char* subject;
	char* body;
	int numAttachments;
	Attachment* attachments;
} Mail;

typedef struct User {
	char name[MAX_NAME_LEN];
	char password[MAX_PASSWORD_LEN];
	Mail* mails;
} User;

typedef struct Message {
	MessageType messageType;
	int dataSize;
	unsigned char* data;
} Message;

void print_error();

void print_error_message(char* message);

int send_message(int targetSocket, Message *message, unsigned int *len);

int recv_message(int sourceSocket, Message *message, unsigned int *len);

int prepare_message_from_string (char* str, Message* message);

int prepare_string_from_message (char** str, Message* message);
