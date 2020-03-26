#include <Windows.h>


BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	UNREFERENCED_PARAMETER(lpReserved);

	if( dwReason == DLL_PROCESS_ATTACH )
	{
		LoadLibrary(TEXT("moufix.dll"));
	}

	return TRUE;
}