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

#define PORT "8000"
#define BACKLOG 10

char* rootpath;

// forward declarations
int createSocket();
void* handleRequest(void* clientsocketfd);

int main(int argc, char *argv[]) {
	if (argc < 2) {
		perror("Please provide a path to serve files");
		return 1;
	}

	rootpath = argv[1];

	// attempt to set up the socket
	int socketfd = createSocket();

	// infinite loop of accepting requests
	while (1) {
		struct sockaddr_storage clientaddr;
		socklen_t clientlen;
		int clientsocketfd = accept(socketfd, (struct sockaddr*) &clientaddr, &clientlen);
		if (clientsocketfd == -1) {
			perror("accept error");
			return 1;
		}

		// create a new thread for the request
		pthread_t id;
		pthread_create(&id, NULL, handleRequest, &clientsocketfd);
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

	// bind
	int bindstatus = bind(socketfd, serverinfo->ai_addr, serverinfo->ai_addrlen);
	if (bindstatus == -1) {
		perror("bind error");
		exit(1);
	}

	// serverinfo is no longer needed, free it
	freeaddrinfo(serverinfo);

	// set up infinite listener
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
	char* request = (char*) malloc(1024);
	recv((long) clientsocketfd, request, sizeof request, 0);
	// extract the path from the request
	strtok(request, " ");
	char* requestpath = strtok(request, " ");
	// prepare the full path by appending the request to the server's root path
	char* path = (char*) malloc(sizeof(rootpath) + sizeof(requestpath));
	strcat(path, rootpath);
	strcat(path, requestpath);

	// try to open the file and get info
	int file = open(path, O_RDONLY);
	if (file == -1) {
		// the file couldn't be found, return a 404 error
		char* message = "HTTP/1.1 404 NOT FOUND\r\nContent-Length:26\r\n\r\nThat file does not exist.";
		send((long) clientsocketfd, message, strlen(message) + 1, 0);
		free(path);
		return NULL;
	}

	// file was found, get the contents and determine size
	struct stat stats;
	fstat(file, &stats);
	char* contents = (char*) malloc(stats.st_size);
	read(file, contents, stats.st_size);
	// prep the return message
	char* message = (char*) malloc(50 + stats.st_size);
	sprintf(message, "HTTP/1.1 200 OK\r\nContentLength:%ld\r\n\r\n%s", stats.st_size, contents);
	send((long) clientsocketfd, message, strlen(message) + 1, 0);

	free(path);
	free(contents);
	free(message);

	return NULL;
}
