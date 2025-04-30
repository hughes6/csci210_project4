
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

struct message {
	char source[50];
	char target[50]; 
	char msg[200]; // message body
};

void terminate(int sig) {
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

int main() {
	int server;
	int target;
	int dummyfd;
	struct message req;
	signal(SIGPIPE,SIG_IGN);
	signal(SIGINT,terminate);
	server = open("serverFIFO",O_RDONLY);
	dummyfd = open("serverFIFO",O_WRONLY);

	while (1) {
		//read requests from serverFIFO
		if (read(server, &req, sizeof(struct message)) != sizeof(struct message)) {
			continue; //skip incomplete reads
		}
		printf("Received a request from %s to send the message %s to %s.\n",req.source,req.msg,req.target);

		//open target FIFO and write the whole message struct to the target FIFO
		target = open(req.target, O_WRONLY);
		if (target < 0) {
				perror("Failed to open target FIFO");
				continue;
		}
		
		//write the whole message struct to the target FIFO
		if (write(target, &req, sizeof(struct message)) != sizeof(struct message)) {
			perror("Failed to write to target FIFO");
		}
	
		close(target);
	}
	close(server);
	close(dummyfd);
	return 0;
}

