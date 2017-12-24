// LibCui.dll
//
// (C)Kuina-chan
//

#include "main.h"

#include <fcntl.h>
#include <io.h>

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID reserved)
{
	UNUSED(hinst);
	UNUSED(reason);
	UNUSED(reserved);
	return TRUE;
}

EXPORT void _init(void* heap, S64* heap_cnt, S64 app_code, const U8* use_res_flags)
{
	Heap = heap;
	HeapCnt = heap_cnt;
	AppCode = app_code;
	UseResFlags = use_res_flags;
	Instance = (HINSTANCE)GetModuleHandle(NULL);

	wprintf(L""); // Open 'stdout'
	_setmode(_fileno(stdout), _O_U8TEXT); // Set the output format to UTF-8.
}

EXPORT void _fin(void)
{
}
