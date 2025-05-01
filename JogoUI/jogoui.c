#include "../wordgame.h"
#include <conio.h>
#include <fcntl.h>  
#include <io.h>     
#include <stdio.h>
#include <tchar.h>
#include <windows.h>

Player player;

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

void ConnectToArbitro(HANDLE* hPipe) {
	DWORD mode;
	BOOL isSuccess;

	while (1) {
		_tprintf_s(_T("Creating File...\n"));
		*hPipe = CreateFile(
			PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
			OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);

		if (*hPipe != INVALID_HANDLE_VALUE)
			break;

		if (GetLastError() != ERROR_PIPE_BUSY) {
			_tprintf_s(_T("[ERROR] Creating File (%d)\n"), GetLastError());
			exit(1);
		}

		_tprintf_s(_T("Waiting Pipe...\n"));
		if (!WaitNamedPipe(PIPE_NAME, 30000)) {
			_tprintf_s(_T("[ERROR] Waiting Pipe (%d)\n"), GetLastError());
			exit(1);
		}
	}

	mode = PIPE_READMODE_MESSAGE;
	isSuccess = SetNamedPipeHandleState(*hPipe, &mode, NULL, NULL);
	if (!isSuccess) {
		_tprintf_s(_T("[ERROR] Setting Pipe Handle State (%d)\n"), GetLastError());
		CloseHandle(*hPipe);
		exit(1);
	}
}

DWORD WINAPI ServerHandlerThread() {
	HANDLE hPipe;
	TCHAR received[WORD_SIZE];
	DWORD bytesReceived;
	BOOL valid;

	_tprintf_s(_T("Waiting Pipe...\n"));
	if (!WaitNamedPipe(PIPE_NAME1, NMPWAIT_WAIT_FOREVER)) {
		_tprintf_s(_T("[ERROR] Waiting Pipe (%d)\n"), GetLastError());
		exit(1);
	}

	_tprintf_s(_T("Creating File...\n"));
	hPipe = CreateFile(PIPE_NAME1, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hPipe == NULL) {
		_tprintf_s(_T("[ERROR] Creating File (%d)\n"), GetLastError());
		exit(1);
	}

	_tprintf_s(_T("Connected To Server...\n"));
	while (1) {
		valid = ReadFile(hPipe, received, sizeof(received) - sizeof(TCHAR), &bytesReceived, NULL);
		received[bytesReceived / sizeof(TCHAR)] = '\0';

		if (!valid || !bytesReceived) {
			_tprintf_s(_T("[ERROR] Reading File (%d)\n"), GetLastError());
			break;
		}

		_tprintf_s(_T("Received %d Bytes: \"%s\"\n"), bytesReceived, received);
	}

	CloseHandle(hPipe);
}

int _tmain(int argc, TCHAR* argv[]) {
	HANDLE hPipe;
	Message sent, received;
	DWORD bytesSent, bytesReceived;
	BOOL isSuccess;

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

	UserValidation(argc, argv);
	ConnectToArbitro(&hPipe);

	HANDLE hServerThread = CreateThread(NULL, 0, ServerHandlerThread, NULL, 0, NULL);
	if (hServerThread == NULL) {
		_tprintf_s(_T("[ERROR] Creating Server Thread (%d)\n"), GetLastError());
		exit(1);
	}

	while (1) {
		// G E T - M E S S A G E
		_tcscpy_s(sent.username, USERNAME_SIZE, player.username);
		_tprintf(_T("Message (\"exit\" to leave): "));
		_fgetts(sent.text, WORD_SIZE, stdin);
		sent.text[_tcslen(sent.text) - 1] = '\0';

		// W R I T E - F I L E
		_tprintf_s(_T("Writing File...\n"));
		isSuccess = WriteFile(hPipe, &sent, sizeof(Message), &bytesSent, NULL);
		if (!isSuccess) {
			_tprintf_s(_T("[ERROR] Writing File (%d)\n"), GetLastError());
			break;
		}
		_tprintf_s(_T("Sent %d Bytes: \"%s\"\n"), bytesSent, sent.text);

		// E X I T - C O N D I T I O N
		if (_tcscmp(_T("exit"), sent.text) == 0)
			break;

		// R E A D - F I L E
		_tprintf_s(_T("Reading File...\n"));
		isSuccess = ReadFile(hPipe, &received, sizeof(Message), &bytesReceived, NULL);
		if (!isSuccess) {
			_tprintf_s(_T("[ERROR] Reading File (%d)\n"), GetLastError());
			break;
		}
		_tprintf_s(_T("Received %d Bytes: \"%s\"\n"), bytesReceived, received.text);
	}

	_tprintf_s(_T("Player Exiting...\n"));

	CloseHandle(hPipe);
	return 0;
}