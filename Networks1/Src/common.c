#include "common.h"

void print_error() {
	printf("error: %s", strerror(errno));
}

int send_all(int targetSocket, Message *message) {
	int total = 0; /* how many bytes we've sent */
	int len = message->size;
	int bytesleft = sizeof(int) * 2; /* how many we have left to send */
	int n;
	unsigned char *buf; /* = message;*/

	while (total < len + sizeof(int) * 2) {
		n = send(targetSocket, buf + total, bytesleft, 0);
		if (n == -1) {
			break;
		}
		total += n;
		bytesleft -= n;

		if (bytesleft == 0) {
			buf = message->data;
			bytesleft = len;
		}
	}

	/* *len = total;  return number actually sent here */

	return n == -1 ? -1 : 0; /* return -1 on failure, 0 on success */
}

