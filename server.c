#include <asm-generic/socket.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#define PORT "8001"
#define BACKLOG 10

char* rootpath;

// forward declarations
int createSocket();
void* handleRequest(void*);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		perror("Please provide a path to serve files");
		return 1;
	}

	rootpath = argv[1];

	// attempt to set up the socket
	int socketfd = createSocket();
	printf("DEBUG: created socket %d\n", socketfd);

	// infinite loop of accepting requests
	while (1) {
		struct sockaddr_storage clientaddr;
		socklen_t clientlen;
		printf("DEBUG: waiting for a connection...\n");
		int clientsocketfd = accept(socketfd, (struct sockaddr*) &clientaddr, &clientlen);
		if (clientsocketfd == -1) {
			perror("accept error");
			return 1;
		}

		// create a new thread for the request
		pthread_t id;
		if (pthread_create(&id, NULL, handleRequest, &clientsocketfd) != 0) {
			perror("thread creation error");
			return 1;
		}
		printf("DEBUG: created thread %ld for client id %d\n", id, clientsocketfd);
	}
}

// network setup
int createSocket() {
	struct addrinfo hints, *serverinfo;

	// prepare for getaddrinfo, memset to clear and reset hints
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status = getaddrinfo(NULL, PORT, &hints, &serverinfo);
	if (status != 0) {
		perror("getaddrinfo error");
		exit(1);
	}

	// create the socket
	int socketfd = socket(serverinfo->ai_family, serverinfo->ai_socktype, serverinfo->ai_protocol);
	if (socketfd == -1) {
		perror("socket error");
		exit(1);
	}

	// set the socket such that the socket may be reused immediately (useful for debugging)
	int option = 1;
	setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

	// bind
	int bindstatus = bind(socketfd, serverinfo->ai_addr, serverinfo->ai_addrlen);
	if (bindstatus == -1) {
		perror("bind error");
		exit(1);
	}

	// serverinfo is no longer needed, free it
	freeaddrinfo(serverinfo);

	// set up listener
	int listenstatus = listen(socketfd, BACKLOG);
	if (listenstatus == -1) {
		perror("listen error");
		exit(1);
	}

	// return the socket file descriptor
	return socketfd;
}

// handle a request
void *handleRequest(void* clientsocketfd) {
	// receive the request
	char request[3000];
	recv(*(int*) clientsocketfd, request, sizeof(request), 0);
	printf("DEBUG: request received:\n%s\n", request);

	// extract the path from the request
	char* savepointer = request;
	strtok_r(savepointer, " ", &savepointer);
	char* requestpath = strtok_r(savepointer, " ", &savepointer);

	// printf("DEBUG: requestpath: %s\n", requestpath);
	// prepare the full path by appending the request to the server's root path
	char path[200];
	sprintf(path, "%s%s", rootpath, requestpath);
	printf("DEBUG: serving: %s\n", path);

	// try to open the file and get info
	int file = open(path, O_RDONLY);
	if (file == -1) {
		// the file couldn't be found, return a 404 error
		printf("DEBUG: 404\n");
		char* message = "HTTP/1.1 404 NOT FOUND\r\nContent-Length:26\r\n\r\nThat file does not exist.";
		printf("DEBUG: message\n%s\n", message);
		send(*(int*) clientsocketfd, message, strlen(message), 0);
		close(*(int*) clientsocketfd);
		return NULL;
	}

	// file was found, get the contents and determine size
	printf("DEBUG: 200\n");
	struct stat stats;
	fstat(file, &stats);
	char contents[stats.st_size];
	int bytesread = read(file, contents, sizeof(contents));
	contents[bytesread] = '\0';
	close(file);

	// prep the return message
	char message[50 + stats.st_size];
	sprintf(message, "HTTP/1.1 200 OK\r\nContentLength: %ld\r\n\r\n%s", stats.st_size, contents);
	printf("DEBUG: message\n%s\n", message);
	send(*(int*) clientsocketfd, message, strlen(message), 0);
	
	// close the connection with the client
	close(*(int*) clientsocketfd);

	return NULL;
}
