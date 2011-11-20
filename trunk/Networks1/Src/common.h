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
	char* name;
	char* password;
	Mail* mails;
} User;

typedef struct Message {
	MessageType messageType;
	int dataSize;
	unsigned char* data;
} Message;

void print_error();

int send_message(int targetSocket, Message *message, unsigned int *len);

int recv_message(int sourceSocket, Message *message, unsigned int *len);
