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
	int size;
	unsigned char* data;
} Message;

void print_error();

/* Inspired by: http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html */
int send_all(int targetSocket, Message *message);
