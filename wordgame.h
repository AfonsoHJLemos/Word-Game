#ifndef WORDGAME_H
#define WORDGAME_H

#include <windows.h>

// Constants
#define BUFFER_SIZE 256
#define USERNAME_SIZE 20
#define MAX_PLAYERS 20
#define MAX_LETTERS 6
#define RHYTHM 3
#define DICTIONARY_SIZE 50
#define WORD_SIZE 50

// Registry Keys
#define REGISTRY_KEY "Software\\WordGame"
#define REGISTRY_MAX_LETTERS "MAXLETTERS"
#define REGISTRY_RHYTHM "RHYTHM"

// Named Pipes
#define PIPE_NAME "\\\\.\\pipe\\WordGame"

// Shared Memory
#define SHARED_MEM_NAME "WordGameSharedMemory"
#define SHARED_MEM_SIZE 1024

// Shared Memory Structure
typedef struct {
	TCHAR letters[12];
	int letterCount;
	TCHAR lastWord[WORD_SIZE];
	int playerCount;
	bool isGameRunning;
} SharedData;

#endif // WORDGAME_H
