struct WordListNode {
	char word[30];
	struct WordListNode* next;
};

struct GameListNode {
	char word[30];
	bool found;
	struct GameListNode* next;
};
