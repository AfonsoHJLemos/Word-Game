#include "../wordgame.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>

Player player;
SharedData* pSharedData;
HANDLE hSharedMemory, hMutex, newLetterEvent;

void ShowMenu() {
	_tprintf_s(_T("\n  W O R D   G A M E - Welcome %s!\n\n"), player.username);
	_fputts(_T(" Commands:\n  :pont - Mostrar pontos\n  :jogs - Mostrar jogadores\n  :sair - Sair do jogo\n\nLetters: "), stdout);

	/*for (int i = 0; i < MAX_LETTERS; i++)
		_tprintf_s(_T("%c "), pSharedData->letters[i]);*/

	_tprintf_s(_T("\n\nInput word or command: "));
}

void UserValidation(int argc, LPTSTR argv[]) {
	if (argc != 2) {
		_tprintf_s(_T("Syntax: %s <username>\n"), argv[0]);
		exit(1);
	}

	DWORD size = _tcslen(argv[1]);
	if (size > USERNAME_SIZE - 1) {
		_tprintf_s(_T("Username is %d characters. Max size is %d characters.\n"), size, USERNAME_SIZE - 1);
		exit(1);
	}

	memset(&player, 0, sizeof(Player));

	_tcsncpy_s(player.username, USERNAME_SIZE, argv[1], USERNAME_SIZE - 1);
	player.username[USERNAME_SIZE - 1] = '\0';
}

void OpenSharedMemory() {
	// Open the shared memory object created by Árbitro
	hSharedMemory = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, SHARED_MEM_NAME);
	if (hSharedMemory == NULL) {
		_tprintf_s(_T("Failed to open shared memory (%d)\n"), GetLastError());
		exit(1);
	}

	// Map the shared memory into the process's address space
	pSharedData = (SharedData*)MapViewOfFile(hSharedMemory, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEM_SIZE);
	if (pSharedData == NULL) {
		_tprintf_s(_T("Failed to map shared memory (%d)\n"), GetLastError());
		CloseHandle(hSharedMemory);
		exit(1);
	}
}

void ConnectToArbitro() {
	_tprintf_s(_T("Connecting to server pipe...\n"));

	while (TRUE) {
		if (!WaitNamedPipe(PIPE_NAME, NMPWAIT_WAIT_FOREVER)) {
			_tprintf(_T("[ERROR] (WaitNamedPipe) Ligar ao Pipe \"%s\"\n"), PIPE_NAME);
			Sleep(1000);
			exit(-1);
		}

		player.pipe = CreateFile(
			PIPE_NAME,
			GENERIC_READ | GENERIC_WRITE, // Read/Write access
			0,                            // No sharing
			NULL,                         // Default security attributes
			OPEN_EXISTING,                // Opens existing pipe
			0,                            // Default attributes
			NULL                          // No template file
		);

		if (player.pipe != INVALID_HANDLE_VALUE) {
			_tprintf_s(_T("Connected to server pipe.\n"));
			break;
		}

		if (GetLastError() != ERROR_PIPE_BUSY) {
			_tprintf_s(_T("Could not open pipe. GLE=%d\n"), GetLastError());
			exit(1);
		}

		// Wait for the pipe to become available
		if (!WaitNamedPipe(PIPE_NAME, 5000)) {
			_tprintf_s(_T("Could not open pipe: 5 second wait timed out.\n"));
			exit(1);
		}
	}
}

void SendMessageToArbitro(TCHAR* message) {
	DWORD bytesWritten;
	WriteFile(player.pipe, message, (lstrlen(message) + 1) * sizeof(TCHAR), &bytesWritten, NULL);
}

void ReadMessageFromArbitro() {
	TCHAR buffer[BUFFER_SIZE];
	DWORD bytesRead;

	if (ReadFile(player.pipe, buffer, sizeof(buffer) - sizeof(TCHAR), &bytesRead, NULL)) {
		buffer[bytesRead / sizeof(TCHAR)] = '\0';
		_tprintf_s(_T("Arbitro: %ls\n"), buffer);
	}
}

DWORD WINAPI ListenForLetters(LPVOID lpParam) {
	newLetterEvent = OpenEvent(EVENT_MODIFY_STATE | SYNCHRONIZE, FALSE, _T("NewLetterEvent"));
	if (newLetterEvent == NULL) {
		_tprintf_s(_T("Failed to open NewLetterEvent (%d)\n"), GetLastError());
		return 1;
	}

	while (TRUE) {
		if (WaitForSingleObject(newLetterEvent, INFINITE) != WAIT_OBJECT_0) {
			_tprintf_s(_T("WaitForSingleObject failed. GLE=%d\n"), GetLastError());
			break;
		}

		_tprintf_s(_T("\nNew letters received:\n"));
		ResetEvent(newLetterEvent);
	}
}

int _tmain(int argc, LPTSTR argv[]) {
#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

	UserValidation(argc, argv);
	ConnectToArbitro();
	//OpenSharedMemory();

	// Letters Thread
	/*HANDLE letterListenerThread = CreateThread(NULL, 0, ListenForLetters, NULL, 0, NULL);
	if (letterListenerThread == NULL) {
		_tprintf_s(_T("Failed to create letter listener thread (%d)\n"), GetLastError());
		exit(1);
	}

	system("cls");*/

	TCHAR input[WORD_SIZE];
	while (TRUE) {
		ShowMenu();

		_fgetts(input, WORD_SIZE, stdin);
		input[_tcslen(input) - 1] = '\0';

		if (_tcscmp(input, _T(":pont")) == 0) {
			_tprintf_s(_T("%c Your points: %d\n"), pSharedData->letters[0], player.points);
		}
		else if (_tcscmp(input, _T(":jogs")) == 0) {
			_tprintf_s(_T("Players in the game:\n"));
			for (int i = 0; i < pSharedData->playerCount; i++)
				_tprintf_s(_T("Player %d: %ls\n"), i + 1, pSharedData->players[i].username);
		}
		else if (_tcscmp(input, _T(":sair")) == 0) {
			break;
		}
		else {
			SendMessageToArbitro(input);
		}
	}

	CloseHandle(player.pipe);
	return 0;
}
