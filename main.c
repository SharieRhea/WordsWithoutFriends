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

// root node for the list of dictionary words
struct WordListNode* dictionaryHead;
// root node for list of game words
struct GameListNode* gameHead;

int main() {
	int max = initialization();
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

	int count = 1;
	char* status;
	struct WordListNode* previousNode = dictionaryHead;
	do {
		struct WordListNode* currentNode = malloc(sizeof(struct WordListNode));
		status = fgets(currentNode->word, 30, file);
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
	printf("All done.\n");
}

void displayWorld() {
	printf("---------------------");
}

void acceptInput() {
	printf("Enter a guess: ");

	char input[20];
	fgets(input, sizeof(input), stdin);

	// convert the input to uppercase for future handling
	for (int i = 0; i < sizeof(input); i++) {
		// remove carriage return and or line feed 
		if (input[i] == '\r' || input[i] == '\n')
			input[i] = '\0';
		input[i] = toupper(input[i]);
	}
	printf("%s\n", input);
}

bool isDone() {
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
