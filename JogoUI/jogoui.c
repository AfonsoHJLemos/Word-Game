#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include "../wordgame.h"

int _tmain(int argc, LPTSTR argv[]) {
	HANDLE hPipe;
	TCHAR buffer[BUFFER_SIZE];
	DWORD nBytes;
	BOOL valid;

#ifdef UNICODE
	_setmode(_fileno(stdin), _O_WTEXT);
	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
#endif

	_tprintf_s(_T("[JOGADOR] (WaitNamedPipe) Esperar pelo Pipe \"%s\"\n"), PIPE_NAME);
	if (!WaitNamedPipe(PIPE_NAME, NMPWAIT_WAIT_FOREVER)) {
		_tprintf(_T("[ERROR] (WaitNamedPipe) Ligar ao Pipe \"%s\"\n"), PIPE_NAME);
		Sleep(1000);
		exit(-1);
	}

	_tprintf_s(_T("[JOGADOR] (CreateFile) Ligar ao Pipe do Árbitro!\n"));
	hPipe = CreateFile(PIPE_NAME, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hPipe == NULL) {
		_tprintf_s(_T("[ERROR] (CreateFile) Ligar ao pipe \"%s\"\n"), PIPE_NAME);
		Sleep(1000);
		exit(-1);
	}

	_tprintf_s(_T("[JOGADOR] Liguei-me!\n"));
	while (1) {
		valid = ReadFile(hPipe, buffer, sizeof(buffer), &nBytes, NULL);
		buffer[nBytes / sizeof(TCHAR)] = '\0';

		if (!valid || !nBytes) {
			_tprintf_s(_T("[ERROR] (ReadFile) %d %d...\n"), valid, nBytes);
			Sleep(1000);
			break;
		}

		_tprintf_s(_T("[JOGADOR] (ReadFile) Recebi %d bytes: \"%s\"\n"), nBytes, buffer);
	}

	CloseHandle(hPipe);

	return 0;
}
