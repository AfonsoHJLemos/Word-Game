#include "../wordgame.h"
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>

void HandlePlayerMessage(Message received, Message* sent) {
	DWORD size = _tcslen(received.text);

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
		_tprintf_s(_T("Received: %d Bytes - \"%s\"\n"), bytesReceived, received.text);

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
		_tprintf_s(_T("Sent: %d Bytes - \"%s\"\n"), bytesSent, sent.text);
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

	while (1) {
		_tprintf_s(_T("Creating Pipe...\n"));
		hPipe = CreateNamedPipe(
			PIPE_NAME, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			MAX_PLAYERS, BUFFER_SIZE, BUFFER_SIZE, 5000, NULL);

		_tprintf_s(_T("Connecting Pipe...\n"));
		isConnected = ConnectNamedPipe(hPipe, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
		if (isConnected) {
			hThread = CreateThread(NULL, 0, PlayerHandlerThread, (LPVOID)hPipe, 0, &threadId);
			if (hThread == NULL) {
				_tprintf_s(_T("[ERROR] Creating Thread (%d)\n"), GetLastError());
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
