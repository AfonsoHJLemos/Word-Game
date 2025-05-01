#include "../wordgame.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>

typedef struct {
	int numPlayers;
	int terminate;
	HANDLE hPipes[MAX_PLAYERS];
	HANDLE hMutex;
} ThreadData;

DWORD WINAPI ServerToPlayerThread(LPVOID param) {
	ThreadData* data = (ThreadData*)param;
	TCHAR sent[WORD_SIZE];
	DWORD bytesSent;

	do {
		_tprintf(_T("Message (\"exit\" to leave): "));
		_fgetts(sent, WORD_SIZE, stdin);
		sent[_tcslen(sent) - 1] = '\0';

		WaitForSingleObject(data->hMutex, INFINITE);
		for (int i = 0; i < data->numPlayers; i++) {
			if (!WriteFile(data->hPipes[i], sent, _tcslen(sent) * sizeof(TCHAR), &bytesSent, NULL)) {
				_tprintf_s(_T("[ERROR] Writing Pipe (%d)\n"), GetLastError());
				exit(1);
			}

			_tprintf_s(_T("Sent %d Bytes: \"%s\"\n"), bytesSent, sent);
		}
		ReleaseMutex(data->hMutex);
	} while (_tcsicmp(sent, _T("exit")));
	data->terminate = 1;

	CreateFile(PIPE_NAME1, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
}

DWORD WINAPI InputThread() {
	ThreadData data;
	HANDLE hPipe, hThread;

	data.numPlayers = 0;
	data.terminate = 0;
	data.hMutex = CreateMutex(NULL, FALSE, NULL);
	if (data.hMutex == NULL) {
		_tprintf_s(_T("[ERROR] Creating Mutex (%d)\n"), GetLastError());
		exit(1);
	}

	hThread = CreateThread(NULL, 0, ServerToPlayerThread, &data, 0, NULL);
	if (hThread == NULL) {
		_tprintf_s(_T("[ERROR] Creating Input To Player Thread (%d)\n"), GetLastError());
		exit(1);
	}

	while (!data.terminate) {
		_tprintf_s(_T("Creating Player Pipe...\n"));
		hPipe = CreateNamedPipe(PIPE_NAME1, PIPE_ACCESS_OUTBOUND,
			PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
			MAX_PLAYERS, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 1000, NULL);
		if (hPipe == INVALID_HANDLE_VALUE) {
			_tprintf_s(_T("[ERROR] Creating Player Pipe (%d)\n"), GetLastError());
			exit(1);
		}

		_tprintf_s(_T("Waiting Player To Connect...\n"));
		if (!ConnectNamedPipe(hPipe, NULL)) {
			_tprintf_s(_T("[ERROR] Player Couldn't Connect (%d)\n"), GetLastError());
			exit(1);
		}

		WaitForSingleObject(data.hMutex, INFINITE);
		data.hPipes[data.numPlayers] = hPipe;
		data.numPlayers++;
		ReleaseMutex(data.hMutex);
	}

	WaitForSingleObject(hThread, INFINITE);

	for (int i = 0; i < data.numPlayers; i++) {
		_tprintf_s(_T("Disconnecting Player Pipes...\n"));
		if (!DisconnectNamedPipe(data.hPipes[i])) {
			_tprintf_s(_T("[ERROR] Disconnection Player Pipe (%d)"), GetLastError());
			exit(1);
		}

		CloseHandle(data.hPipes[i]);
	}
}

void HandlePlayerMessage(Message received, Message* sent) {
	DWORD size = _tcslen(received.text);

	_tcscpy_s(sent->username, USERNAME_SIZE, received.username);
	for (int i = 0; i < size; i++)
		sent->text[i] = _totupper(received.text[i]);
	sent->text[size] = '\0';
}

DWORD WINAPI PlayerHandlerThread(LPVOID param) {
	HANDLE hPipe = (HANDLE)param;
	Message received, sent;
	DWORD bytesReceived, bytesSent;
	BOOL isSuccess;

	while (1) {
		// R E A D - F I L E
		_tprintf_s(_T("Reading File...\n"));
		isSuccess = ReadFile(hPipe, &received, sizeof(Message), &bytesReceived, NULL);
		if (!isSuccess) {
			_tprintf_s(_T("[ERROR] Reading File (%d)\n"), GetLastError());
			break;
		}
		_tprintf_s(_T("[%s] Sent %d Bytes: \"%s\"\n"), received.username, bytesReceived, received.text);

		// E X I T - C O N D I T I O N
		if (_tcscmp(received.text, _T("exit")) == 0) {
			_tprintf_s(_T("Player Exiting...\n"));
			FlushFileBuffers(hPipe);
			DisconnectNamedPipe(hPipe);
			CloseHandle(hPipe);
			return 0;
		}

		HandlePlayerMessage(received, &sent);

		// W R I T E - F I L E
		_tprintf_s(_T("Writing File...\n"));
		isSuccess = WriteFile(hPipe, &sent, sizeof(Message), &bytesSent, NULL);
		if (!isSuccess) {
			_tprintf_s(_T("[ERROR] Writing File (%d)\n"), GetLastError());
			break;
		}
		_tprintf_s(_T("[%s] Received %d Bytes: \"%s\"\n"), received.username, bytesSent, sent.text);
	}

	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);
	exit(1);
}

int _tmain(int argc, LPTSTR argv[]) {
	HANDLE hPipe, hThread;
	BOOL isConnected;
	DWORD threadId;

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

	// Create Input Thread
	HANDLE hInputThread = CreateThread(NULL, 0, InputThread, NULL, 0, NULL);
	if (hInputThread == NULL) {
		_tprintf_s(_T("[ERROR] Creating Input Thread (%d)\n"), GetLastError());
		exit(1);
	}

	while (1) {
		_tprintf_s(_T("Creating Pipe...\n"));
		hPipe = CreateNamedPipe(
			PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			MAX_PLAYERS, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 5000, NULL);

		_tprintf_s(_T("Connecting Pipe...\n"));
		isConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (isConnected) {
			hThread = CreateThread(NULL, 0, PlayerHandlerThread, (LPVOID)hPipe, 0, &threadId);
			if (hThread == NULL) {
				_tprintf_s(_T("[ERROR] Creating Player Handler Thread (%d)\n"), GetLastError());
				exit(1);
			}

			CloseHandle(hThread);
		}
		else {
			CloseHandle(hPipe);
		}
	}

	return 0;
}
