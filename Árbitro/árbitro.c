#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include "../wordgame.h"

typedef struct {
	int numPlayers;
	int terminate;
	HANDLE hPipes[MAX_PLAYERS];
	HANDLE hMutex;
} ThreadData;

DWORD WINAPI ThreadMessages(LPVOID param) {
	ThreadData* data = (ThreadData*)param;
	TCHAR buffer[BUFFER_SIZE];
	DWORD nBytes;

	do {
		_tprintf_s(_T("[ÁRBITRO] Frase (\"fim\" para sair): "));
		_fgetts(buffer, BUFFER_SIZE, stdin);
		buffer[_tcslen(buffer) - 1] = '\0';

		WaitForSingleObject(data->hMutex, INFINITE);
		for (int i = 0; i < data->numPlayers; i++) {
			if (!WriteFile(data->hPipes[i], buffer, _tcslen(buffer) * sizeof(TCHAR), &nBytes, NULL)) {
				_tprintf_s(_T("[ERROR] (WriteFile) Escrever no pipe!\n"));
				Sleep(1000);
				exit(-1);
			}

			_tprintf_s(_T("[ÁRBITRO] (WriteFile) Enviei %d bytes ao jogador [%d]!\n"), nBytes, i);
		}
		ReleaseMutex(data->hMutex);
	} while (_tcsicmp(buffer, _T("fim")));

	data->terminate = 1;
	CreateFile(PIPE_NAME, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	return 0;
}

int _tmain(int argc, LPTSTR argv[]) {
	ThreadData data;
	HANDLE hPipe, hThread;

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

	data.numPlayers = 0;
	data.terminate = 0;
	data.hMutex = CreateMutex(NULL, FALSE, NULL);
	if (data.hMutex == NULL) {
		_tprintf_s(_T("[ERROR] (CreateMutex) Criar Mutex!\n"));
		Sleep(1000);
		exit(-1);
	}

	hThread = CreateThread(NULL, 0, ThreadMessages, &data, 0, NULL);
	if (hThread == NULL) {
		_tprintf_s(_T("[ERROR] (CreateThread) Criar Thread!\n"));
		Sleep(1000);
		exit(-1);
	}

	while (!data.terminate) {
		_tprintf_s(_T("[ÁRBITRO] (CreateNamedPipe) Criar uma cópia do Pipe \"%s\"\n"), PIPE_NAME);

		hPipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_OUTBOUND,
			PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
			MAX_PLAYERS, BUFFER_SIZE * sizeof(TCHAR), BUFFER_SIZE * sizeof(TCHAR), 1000, NULL);
		if (hPipe == INVALID_HANDLE_VALUE) {
			_tprintf_s(_T("[ERROR] (CreateNamedPipe) Criar Pipe!\n"));
			Sleep(1000);
			exit(-1);
		}

		_tprintf_s(_T("[ÁRBITRO] (ConnectNamedPipe) Esperar ligação de um jogador...\n"));
		if (!ConnectNamedPipe(hPipe, NULL)) {
			_tprintf_s(_T("[ERROR] (ConnectNamedPipe) Ligar ao jogador!\n"));
			Sleep(1000);
			exit(-1);
		}

		WaitForSingleObject(data.hMutex, INFINITE);
		data.hPipes[data.numPlayers] = hPipe;
		data.numPlayers++;
		ReleaseMutex(data.hMutex);
	}

	WaitForSingleObject(hThread, INFINITE);

	for (int i = 0; i < data.numPlayers; i++) {
		_tprintf_s(_T("[ÁRBITRO] (DisconnectNamedPipe) Desligar o pipe!\n"));
		if (!DisconnectNamedPipe(data.hPipes[i])) {
			_tprintf_s(_T("[ERROR] (DisconnectNamedPipe) Desligar o pipe!"));
			Sleep(1000);
			exit(-1);
		}

		CloseHandle(data.hPipes[i]);
	}

	return 0;
}
