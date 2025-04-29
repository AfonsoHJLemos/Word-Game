#include "../wordgame.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>

// Global variables
GameState game;
SharedData* pSharedData;
HANDLE hPipe, hSharedMemory, hMutex, newLetterEvent;

void InitializeGame() {
	memset(&game, 0, sizeof(GameState));
	game.leaderIndex = -1;
}

void ReadRegistryKeys() {
	HKEY hKey;
	DWORD value = NULL;
	DWORD size = sizeof(DWORD);

	if (RegCreateKeyEx(HKEY_CURRENT_USER, REGISTRY_KEY, 0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &hKey, NULL) != ERROR_SUCCESS) {
		_tprintf_s(_T("Failed to open registry key. Using default values.\n"));
		game.maxLetters = MAX_LETTERS;
		game.rhythm = RHYTHM;
	}
	else {
		// Max Letters
		if (RegQueryValueEx(hKey, REGISTRY_MAX_LETTERS, NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
			game.maxLetters = (int)value;
			if (game.maxLetters > 12)
				game.maxLetters = 12;
		}
		else {
			value = MAX_LETTERS;
			RegSetValueEx(hKey, REGISTRY_MAX_LETTERS, 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
		}

		// Rhythm
		if (RegQueryValueEx(hKey, REGISTRY_RHYTHM, NULL, NULL, (LPBYTE)&value, &size) == ERROR_SUCCESS) {
			game.rhythm = (int)value;
			if (game.rhythm < 1)
				game.rhythm = 1;
		}
		else {
			value = RHYTHM;
			RegSetValueEx(hKey, REGISTRY_RHYTHM, 0, REG_DWORD, (const BYTE*)&value, sizeof(DWORD));
		}
	}

	RegCloseKey(hKey);
}

void CreateSharedMemory() {
	// Create shared memory
	hSharedMemory = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, SHARED_MEM_SIZE, SHARED_MEM_NAME);
	if (hSharedMemory == NULL) {
		_tprintf_s(_T("CreateFileMapping failed (%d)\n"), GetLastError());
		exit(1);
	}

	// Map shared memory
	pSharedData = (SharedData*)MapViewOfFile(hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
	if (pSharedData == NULL) {
		_tprintf_s(_T("MapViewOfFile failed (%d)\n"), GetLastError());
		CloseHandle(hSharedMemory);
		exit(1);
	}

	// Initialize shared data
	memset(pSharedData, 0, sizeof(SharedData));
	for (int i = 0; i < game.maxLetters; i++)
		pSharedData->letters[i] = _T('_');
	pSharedData->isGameRunning = TRUE;

	_tcsncpy_s(pSharedData->players[1].username, USERNAME_SIZE, _T("Afonso\0"), USERNAME_SIZE - 1);

	_tprintf_s(_T("Shared memory created\n"));
}

void AddLetter() {
	TCHAR letter = 'a' + rand() % 26;

	// Find an empty slot
	int slot = -1;
	for (int i = 0; i < game.maxLetters; i++)
		if (pSharedData->letters[i] == _T('_')) {
			slot = i;
			break;
		}

	// Is full
	if (slot < 0) {
		static int index = 0;
		pSharedData->letters[index] = letter;
		index = (index + 1) % game.maxLetters;
	}
	else {
		pSharedData->letters[slot] = letter;
		pSharedData->letterCount++;
	}
}

DWORD WINAPI LetterGeneratorThread(LPVOID lpParam) {
	while (TRUE) {
		Sleep(1000 * game.rhythm);

		if (pSharedData->isGameRunning) {
			WaitForSingleObject(hMutex, INFINITE);

			AddLetter();
			for (int i = 0; i < game.maxLetters; i++)
				_tprintf_s(_T("%c "), pSharedData->letters[i]);
			_tprintf_s(_T("\n"));

			SetEvent(newLetterEvent);
			ResetEvent(newLetterEvent);

			ReleaseMutex(hMutex);
		}
	}
}

DWORD WINAPI PlayerHandlerThread(LPVOID lpParam) {
	TCHAR word[WORD_SIZE];
	BOOL valid, nBytes;

	while (TRUE) {
		valid = ReadFile(hPipe, word, sizeof(word) - sizeof(TCHAR), &nBytes, NULL);
		word[nBytes / sizeof(TCHAR)] = '\0';

		if (!valid || !nBytes) {
			_tprintf_s(_T("[ERROR] (ReadFile) %d %d...\n"), valid, nBytes);
			Sleep(1000);
			break;
		}

		_tprintf_s(_T("%s\n"), word);
	}
}

void CreateGameNamedPipe() {
	while (TRUE) {
		hPipe = CreateNamedPipe(
			PIPE_NAME,
			PIPE_ACCESS_DUPLEX, // Read/Write access
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, // Message type pipe
			PIPE_UNLIMITED_INSTANCES, // Unlimited instances
			BUFFER_SIZE, // Output buffer size
			BUFFER_SIZE, // Input buffer size
			0, // Default timeout
			NULL // Default security attributes
		);

		if (hPipe == NULL || hPipe == INVALID_HANDLE_VALUE) {
			_tprintf_s(_T("CreateNamedPipe failed (%d)\n"), GetLastError());
			exit(1);
		}

		_tprintf_s(_T("Waiting for player connection...\n"));
		if (ConnectNamedPipe(hPipe, NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
			_tprintf_s(_T("Client connected to pipe\n"));

			HANDLE hThread = CreateThread(NULL, 0, PlayerHandlerThread, (LPVOID)hPipe, 0, NULL);
			if (hThread == NULL) {
				_tprintf_s(_T("CreateThread failed (%d)\n"), GetLastError());
				CloseHandle(hPipe);
			}
			else {
				CloseHandle(hThread);
			}
		}
		else {
			CloseHandle(hPipe);
		}
	}
}

void CreateGameMutex() {
	hMutex = CreateMutex(NULL, FALSE, _T("WordGameMutex"));
	if (hMutex == NULL) {
		_tprintf_s(_T("CreateMutex failed (%d).\n"), GetLastError());
		exit(1);
	}
}

void Cleanup() {
	CloseHandle(hPipe);
	UnmapViewOfFile(pSharedData);
	CloseHandle(hSharedMemory);
	CloseHandle(hMutex);
}

int _tmain(int argc, LPTSTR argv[]) {
#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

	InitializeGame();
	ReadRegistryKeys();

	/*CreateSharedMemory();

	hMutex = CreateMutex(NULL, FALSE, _T("WordGameMutex"));
	if (hMutex == NULL) {
		_tprintf_s(_T("CreateMutex error (%d)\n"), GetLastError());
		exit(1);
	}

	newLetterEvent = CreateEvent(NULL, TRUE, FALSE, _T("NewLetterEvent"));
	if (newLetterEvent == NULL) {
		_tprintf_s(_T("CreateEvent error (%d)\n"), GetLastError());
		exit(1);
	}

	HANDLE letterThread = CreateThread(NULL, 0, LetterGeneratorThread, NULL, 0, NULL);
	if (letterThread == NULL) {
		_tprintf_s(_T("CreateThread error (%d)\n"), GetLastError());
		exit(1);
	}*/

	CreateGameNamedPipe();

	Cleanup();

	return 0;
}
