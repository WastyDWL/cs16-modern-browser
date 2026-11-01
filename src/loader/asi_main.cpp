#include <windows.h>

extern "C" int __stdcall RIB_Main(void*, unsigned, void*, void*, void*) { return 1; }

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
