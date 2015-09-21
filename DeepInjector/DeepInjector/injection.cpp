#include "stdafx.h"
#include <TLHELP32.H>
#include "injection.h"
#include "CNtDriverControl.h"

#define OP_INJECT	1
#define OP_EJECT	2

#define MakePtr( cast, ptr, addValue ) (cast)( (DWORD_PTR)(ptr) + (DWORD_PTR)(addValue))
#define MakeDelta(cast, x, y) (cast) ( (DWORD_PTR)(x) - (DWORD_PTR)(y))

extern CNtDriverControl Driver;



void TruncPath(char *szExt)
{
	for(int i = (int)strlen(szExt), cur = 0; i > 0; i--)
	{
		if(szExt[i] == '\\')
		{
			szExt[i + 1] = 0;
			break;
		}
	}
}

short GetOneThreadID(short nPID)
{
	HANDLE hThreadSnap = NULL;
	THREADENTRY32 te32 = {0}; 

	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
		return 0;

	memset(&te32, 0, sizeof(THREADENTRY32));
	te32.dwSize = sizeof(THREADENTRY32);

	if( Thread32First( hThreadSnap, &te32 ) )
	{
		do
		{
			if (te32.th32ThreadID && te32.th32OwnerProcessID == nPID)
			{
				CloseHandle (hThreadSnap);
				return te32.th32ThreadID;
			}

		} while ( Thread32Next( hThreadSnap, &te32 ) );
	}

	CloseHandle (hThreadSnap); 
	return 0;
}


HMODULE GetRemoteModuleHandle(const char *module, short nPID)
{
	HANDLE tlh = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, nPID);
	MODULEENTRY32 modEntry;
	modEntry.dwSize = sizeof(MODULEENTRY32);

	Module32First(tlh, &modEntry);
	do
	{
		if(!stricmp(module, modEntry.szModule))
		{
			CloseHandle(tlh);
			return modEntry.hModule;
		}
	}
	while(Module32Next(tlh, &modEntry));
	CloseHandle(tlh);

	return NULL;
}

FARPROC GetRemoteProcAddress(const char *module, const char *func, short nPID)
{
	HMODULE remoteMod = GetRemoteModuleHandle(module, nPID);
	HMODULE localMod = GetModuleHandle(module);

	//	If the module isn't already loaded, we load it, but since many of the 
	//	modules we'll probably be loading will do nasty things like modify
	//	memory and hook functions, we use the DONT_RESOLVE_DLL_REFERENCES flag,
	//	so that LoadLibraryEx only loads the dll, but doesn't execute it.
	if(!localMod) localMod = LoadLibraryEx(module, NULL, DONT_RESOLVE_DLL_REFERENCES);

	//	Account for potential differences in base address
	//	of modules in different processes.
	int delta = MakeDelta(int, remoteMod, localMod);

	FARPROC LocalFunctionAddress = GetProcAddress(localMod, func);

	return MakePtr(FARPROC, LocalFunctionAddress, delta);
}

BOOL Inject(short nPID, const char* szLibPath)
{

	DWORD dwLdrLoadDdll = (DWORD)GetRemoteProcAddress("ntdll.dll", "LdrLoadDll", nPID);

	USHORT nTID = GetOneThreadID(nPID);

	if(!Driver.SendCommand(OP_INJECT, (DWORD)nPID, (DWORD) nTID, (DWORD)szLibPath, (DWORD)dwLdrLoadDdll, 0, 0, 0))
		return FALSE;

	return TRUE;
}


BOOL Eject(short nPID, const char* szLibPath)
{

	DWORD dwLdrLoadDdll = (DWORD)GetRemoteProcAddress("ntdll.dll", "LdrUnloadDll", nPID);

	if(!Driver.SendCommand(OP_EJECT, (DWORD)nPID, (DWORD)szLibPath, 0, 0, 0, 0, 0))
		return FALSE;

	return TRUE;
}