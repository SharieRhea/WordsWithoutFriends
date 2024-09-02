#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include<stdbool.h>
#include<time.h>
#include<ctype.h>

int initialization();
void gameLoop();
void teardown();
void displayWorld();
void acceptInput();
bool isDone();

int main() {
	initialization();
	gameLoop();
	teardown();
	return 0;
}

int initialization() {
	srand(time(NULL));
	return 0;
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
