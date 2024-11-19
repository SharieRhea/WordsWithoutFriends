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
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include "structs.h"

// root node for the list of dictionary words
struct WordListNode* dictionaryHead;
// root node for list of game words
struct GameListNode* gameHead;
// the master word for the current game
char* master;


#define PORT "8001"
#define BACKLOG 10

char* rootpath;

// forward declarations
int createSocket();
void* handleRequest(void*);
int initialization();
void gameLoop();
void teardown();
void displayWorld();
void acceptInput(char*);
bool isDone();
char* getRandomWord(int);
void findWords(char*);
void getLetterDistribution(char*, int[]);
void cleanUpGameListNodes();
void cleanUpWordListNodes();

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

	printf("DEBUG: requestpath: %s\n", requestpath);
	// prepare the full path by appending the request to the server's root path
	char path[200];
	sprintf(path, "%s%s", rootpath, requestpath);
	printf("DEBUG: serving: %s\n", path);

	if (strcmp("/wordswithoutfriends", requestpath) == 0) {
		// no guess, startup a game
		int max = initialization();
		master = getRandomWord(max);
		findWords(master);
		displayWorld();
		sprintf(path, "%s", "output.html");
	}
	else {
		savepointer = requestpath;
		if (strcmp("/wordswithoutfriends?move", strtok_r(savepointer, "=", &savepointer)) == 0) {
			char* move = savepointer;

			// check to make sure a game has been started
			if (gameHead == NULL) {
				int max = initialization();
				master = getRandomWord(max);
				findWords(master);
			}

			acceptInput(move);
			if (isDone()) {
				int output = open("output.html", O_WRONLY | O_TRUNC);
				if (output == -1) {
					perror("Unable to open output.html");
					exit(1);
				}
				char buffer[4096];
				sprintf(buffer, "<html>\n\t<body>\n\t\t<p>Congrats! You solved it!\n\t\t<a href=\"wordswithoutfriends\">Another?</a>\n\t<body>\n<html>");
				write(output, buffer, strlen(buffer));
				close(output);
			}
			else 
				displayWorld();
			sprintf(path, "%s", "output.html");
		}
	}

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

int initialization() {
	srand(time(NULL));

	// open file and ensure it exists
	FILE* file = fopen("dictionary.txt", "r");
	if (file == NULL) {
		printf("Could not open dictionary.txt. Perhaps it is missing?\n");
		exit(1);
	}
	
	// read each word and build linked list
	dictionaryHead = malloc(sizeof(struct WordListNode));
	if (dictionaryHead == NULL) {
		printf("Malloc failed.\n");
		exit(1);
	}
	if (fgets(dictionaryHead->word, 30, file) == NULL) {
		printf("dictionary.txt is empty.");
		exit(1);
	}

	// clean the \r\n
	for (int i = 0; i < sizeof(dictionaryHead->word); i++) {
		if (dictionaryHead->word[i] == '\n' || dictionaryHead->word[i] == '\r')
			dictionaryHead->word[i] = '\0';
	}

	int count = 1;
	char* status;
	struct WordListNode* previousNode = dictionaryHead;
	do {
		struct WordListNode* currentNode = malloc(sizeof(struct WordListNode));
		status = fgets(currentNode->word, 30, file);
		// clean the \r\n
		for (int i = 0; i < sizeof(currentNode->word); i++) {
			if (currentNode->word[i] == '\n' || dictionaryHead->word[i] == '\r')
				currentNode->word[i] = '\0';
		}
		previousNode->next = currentNode;
		previousNode = currentNode;
		count++;
		if (currentNode == NULL) {
			printf("Malloc failed.\n");
			exit(1);
		} 
	} while (status != NULL);

	fclose(file);

	// return the number of words to be used later
	return count;
}

void teardown() {
	displayWorld();
	cleanUpGameListNodes();
	cleanUpWordListNodes();
	printf("All done.\n");
}

void displayWorld() {
	// open a file to store for ease
	int output = open("output.html", O_WRONLY | O_TRUNC);
	if (output == -1) {
		perror("Unable to open output.html");
		exit(1);
	}
	char buffer[4096];
	sprintf(buffer, "<html>\n\t<body>\n\t\t<p>");
	// use the distribution of the master word to print its sorted letters
	int distribution[26] = { 0 };
	getLetterDistribution(master, distribution);
	for (int i = 0; i < sizeof(distribution) / 4; i++) {
		for (int j = 0; j < distribution[i]; j++) {
			char letter[3];
			sprintf(letter, "%c ", i + 65);
			strcat(buffer, letter);
		}
	}
	strcat(buffer, "\n\t\t<p>---------------------------------------------------");

	// print each word if it has been found, hangman style otherwise
	struct GameListNode* node = gameHead;
	while (node != NULL) {
		if (node->found) {
			strcat(buffer, "\n\t\t<p>FOUND: ");
			strcat(buffer, node->word);
		}
		else {
			strcat(buffer, "\n\t\t<p>");
			int length = strlen(node->word);
			for (int i = 0; i < length; i++) {
				strcat(buffer, "_ ");
			}
			char numberOfLetters[6];
			sprintf(numberOfLetters, " (%d)", length);
			strcat(buffer, numberOfLetters);
		}
		if (node->next == NULL) {
			break;
		}
		node = node->next;
	}
	strcat(buffer, "\n\t\t<form submit=\"wordswithoutfriends\"><input type=\"text\" name=move autofocus></input></form>\n\t<body>\n<html>");
	strcat(buffer, "\0");
	write(output, buffer, strlen(buffer));
	close(output);
}

void acceptInput(char* input) {
	// convert the input to uppercase for future handling
	for (int i = 0; i < strlen(input); i++) {
		// remove carriage return and or line feed 
		if (input[i] == '\r' || input[i] == '\n')
			input[i] = '\0';
		input[i] = toupper(input[i]);
	}
	printf("DEBUG: accepting guess %s\n", input);
	
	// compare to each word in game list
	struct GameListNode* node = gameHead;
	while (node != NULL) {
		if (strcmp(input, node->word) == 0) {
			printf("DEBUG: word found %s\n", input);
			node->found = true;
		}
		node = node->next;
	}
}

bool isDone() {
	struct GameListNode* node = gameHead;
	while (node != NULL) {
		if (!node->found)
			return false;
		node = node->next;
	}
	return true;
}

void getLetterDistribution(char word[], int distribution[]) {
	for (int i = 0; i < strlen(word); i++) {
		distribution[word[i] - 65]++;
	}
}

bool compareCounts(int candidate[], int bank[]) {
	// if any letter in the candidate has more occurences than the bank,
	// it cannot be made
	for (int i = 0; i < 26; i++) {
		if (candidate[i] > bank[i]) 
			return false;
	}
	return true;
}

char* getRandomWord(int max) {
	int entry = rand() % max;
	// walk the linked list to the chosen entry
	struct WordListNode* node = dictionaryHead;
	for (int i = 0; i < entry; i++) {
		node = node->next;
	}
	// continue looking until we find a word longer than 6 characters
	while (node->next != NULL && strlen(node->word) < 7) {
		node = node->next;
	}
	// check that we ended up with something longer than 6 characters
	// if not, we hit end of list, try again
	if (strlen(node->word) < 7)
		return getRandomWord(max);
	return node->word;
}

void findWords(char* master) {
	struct WordListNode* dictionaryNode = dictionaryHead;
	gameHead = malloc(sizeof(struct GameListNode));
	struct GameListNode* gameNode = gameHead;
	bool firstWordInitialized = false;

	int bank[26] = { 0 }; 
	getLetterDistribution(master, bank);
	
	do {
		int distribution[26] = { 0 };
		getLetterDistribution(dictionaryNode->word, distribution);
		if (compareCounts(distribution, bank)) {
			// case where we are adding the first word
			if (!firstWordInitialized) {
				strcpy(gameHead->word, dictionaryNode->word);
				gameHead->next = NULL;
				firstWordInitialized = true;
			}
			// appending to linked list
			else {
				struct GameListNode* newGameNode = malloc(sizeof(struct GameListNode));
				strcpy(newGameNode->word, dictionaryNode->word);
				newGameNode->next = NULL;
				gameNode->next = newGameNode;
				gameNode = gameNode->next;
			}
		}
		dictionaryNode = dictionaryNode->next;
	} while (dictionaryNode->next != NULL);
}

void cleanUpGameListNodes() {
	struct GameListNode* node = gameHead;
	while (node != NULL) {
		struct GameListNode* nodeToFree = node;
		node = node->next;
		free(nodeToFree);
	}
}

void cleanUpWordListNodes() {
	struct WordListNode* node = dictionaryHead;
	while (node != NULL) {
		struct WordListNode* nodeToFree = node;
		node = node->next;
		free(nodeToFree);
	}
}
