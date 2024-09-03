#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<stdbool.h>
#include<time.h>
#include<ctype.h>

#include "structs.h"

int initialization();
void gameLoop();
void teardown();
void displayWorld();
void acceptInput();
bool isDone();
char* getRandomWord(int);
void findWords(char*);
void getLetterDistribution(char*, int[]);
void cleanUpGameListNodes();
void cleanUpWordListNodes();

// root node for the list of dictionary words
struct WordListNode* dictionaryHead;
// root node for list of game words
struct GameListNode* gameHead;
// the master word for the current game
char* master;

int main() {
	int max = initialization();
	master = getRandomWord(max);
	findWords(master);
	gameLoop();
	teardown();
	return 0;
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

void gameLoop() {
	while (!isDone()) {
		displayWorld();
		acceptInput();
	}
}

void teardown() {
	displayWorld();
	cleanUpGameListNodes();
	cleanUpWordListNodes();
	printf("All done.\n");
}

void displayWorld() {
	// use the distribution of the master word to print its sorted letters
	int distribution[26] = { 0 };
	getLetterDistribution(master, distribution);
	for (int i = 0; i < sizeof(distribution) / 4; i++) {
		for (int j = 0; j < distribution[i]; j++) {
			printf("%c ", i + 65);
		}
	}
	printf("\n-------------------------\n");

	// print each word if it has been found, hangman style otherwise
	struct GameListNode* node = gameHead;
	while (node != NULL) {
		if (node->found) {
			printf("FOUND: %s\n", node->word);
		}
		else {
			int length = strlen(node->word);
			for (int i = 0; i < length; i++) {
				printf("_");	
			}
			printf(" (%d)\n", length);
		}
		node = node->next;
	}
	printf("\n");
}

void acceptInput() {
	printf("Enter a guess: ");

	char input[30];
	fgets(input, sizeof(input), stdin);

	// convert the input to uppercase for future handling
	for (int i = 0; i < sizeof(input); i++) {
		// remove carriage return and or line feed 
		if (input[i] == '\r' || input[i] == '\n')
			input[i] = '\0';
		input[i] = toupper(input[i]);
	}
	
	// compare to each word in game list
	struct GameListNode* node = gameHead;
	while (node != NULL) {
		if (strcmp(input, node->word) == 0)
			node->found = true;
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
				firstWordInitialized = true;
			}
			// appending to linked list
			else {
				struct GameListNode* newGameNode = malloc(sizeof(struct GameListNode));
				strcpy(newGameNode->word, dictionaryNode->word);
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
