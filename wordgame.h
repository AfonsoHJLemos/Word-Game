#ifndef WORDGAME_H
#define WORDGAME_H

#include <windows.h>

// Constants
#define BUFFER_SIZE 256
#define USERNAME_SIZE 21
#define MAX_PLAYERS 20
#define MAX_LETTERS 6
#define RHYTHM 3
#define DICTIONARY_SIZE 50
#define WORD_SIZE 21

// Registry Keys
#define REGISTRY_KEY _T("Software\\TrabSO2")
#define REGISTRY_MAX_LETTERS _T("MAXLETTERS")
#define REGISTRY_RHYTHM _T("RHYTHM")

// Named Pipes
#define PIPE_NAME _T("\\\\.\\pipe\\WordGame")

// Shared Memory
#define SHARED_MEM_NAME _T("WordGameSharedMemory")
#define SHARED_MEM_SIZE 1024

// Player structure
typedef struct {
	TCHAR username[USERNAME_SIZE];
	int points;
	BOOL isBot;
	BOOL isActive;
	HANDLE pipe;
} Player;

// Shared Memory Structure
typedef struct {
	TCHAR letters[12];
	int letterCount;
	TCHAR lastWord[WORD_SIZE];
	Player players[MAX_PLAYERS];
	int playerCount;
	BOOL isGameRunning;
} SharedData;

// Game state
typedef struct {
	int maxLetters;
	int rhythm;
	int leaderIndex;
} GameState;

#endif // WORDGAME_H
