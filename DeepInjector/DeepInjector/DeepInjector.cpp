// DeepInjector.cpp : définit le point d'entrée pour l'application.
//

#include "stdafx.h"
#include "DeepInjector.h"

#include <TLHELP32.H>
#include <stdio.h>
#include <commdlg.h>

#include "injection.h"
#include "CNtDriverControl.h"

// Variables globales :
HINSTANCE hInst;
HWND hWnd;
HWND m_hDlg = NULL;

char szTargetProcess[MAX_PATH] = {0}; 
char szDLLToInject[MAX_PATH] = {0};

CNtDriverControl Driver;


LRESULT CALLBACK MainDlgProc( HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	hInst = hInstance;

	char szFolder[MAX_PATH];
	GetModuleFileName(hInstance, szFolder, MAX_PATH);

	TruncPath(szFolder);
	strcat_s(szFolder, MAX_PATH, "inject.sys");

	if(Driver.Open(szFolder, "inject"))
	{
		if(Driver.Load("\\\\.\\inject"))
		{
			DialogBox( hInstance, MAKEINTRESOURCE(IDD_MAIN), hWnd, (DLGPROC)MainDlgProc );
			Driver.Unload();
		}
		Driver.Close();
	}
	return 0;
}


UINT APIENTRY OFNHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam) 
{ 
	if (WM_INITDIALOG==uiMsg) 
	{ 
		HWND hOK=GetDlgItem(GetParent(hdlg), IDOK); 
		SetWindowText(hOK, "Load"); 
		return TRUE; 
	} 
	return FALSE; 
} 

BOOL DoFileDialog(LPSTR lpszFilename, LPSTR lpzFilter, LPSTR lpzExtension) 
{ 
	OPENFILENAME ofn; 

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn); 
	ofn.lpstrFile = lpszFilename; 
	ofn.nMaxFile = MAX_PATH; 
	ofn.lpstrFilter = lpzFilter;
	ofn.lpstrDefExt = lpzExtension;
	ofn.Flags = OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_ENABLEHOOK|OFN_EXPLORER|OFN_ENABLEHOOK;; 
	ofn.lpfnHook = OFNHookProc; 

	return GetOpenFileName(&ofn); 
} 


void PopulateProcessesCombobox(HWND hComboboxProcess)
{
	PROCESSENTRY32 PE;
	HANDLE SnapShot;

	SnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPALL,NULL);
	if (SnapShot != NULL)
	{
		SendMessage(hComboboxProcess, CB_RESETCONTENT, 0, 0);
		PE.dwSize = sizeof(PROCESSENTRY32);
		Process32First(SnapShot,&PE);
		while (Process32Next (SnapShot,&PE))
		{
			char cString[MAX_PATH + 7] = {0};
			sprintf_s(cString, MAX_PATH + 7, "%ld %s", PE.th32ProcessID, PE.szExeFile);
			SendMessage(hComboboxProcess, CB_INSERTSTRING, 0, (LPARAM)cString);
		}
		CloseHandle(SnapShot);
	}
}


LRESULT CALLBACK MainDlgProc( HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam )
{
	int nPID = 0;
	m_hDlg = hDlg;
	switch(Msg)
	{
	case WM_INITDIALOG:
		{
			
			return TRUE;
		}
	case WM_COMMAND:
		switch(LOWORD(wParam)) 
		{

		case IDC_COMBO_PROCESS:
			
			PopulateProcessesCombobox(GetDlgItem(hDlg, IDC_COMBO_PROCESS));

			return TRUE;

		case IDC_BUTTON_BROWSE:

			if(DoFileDialog(szDLLToInject, "DLL Files (*.dll)\0*.dll\0All Files (*.*)\0*.*\0", "dll") == IDOK) 
			{
				SetDlgItemText(hDlg, IDC_EDIT_DLL, szDLLToInject);
			} 
			return TRUE;

		case IDC_BUTTON_INJECT:

			if(GetDlgItemText(hDlg, IDC_COMBO_PROCESS, szTargetProcess, MAX_PATH)
				&& GetDlgItemText(hDlg, IDC_EDIT_DLL, szDLLToInject, MAX_PATH))
			{
				nPID = atoi(szTargetProcess);
				Inject(nPID, szDLLToInject);
			}
			return TRUE;

		case IDC_BUTTON_EJECT:

			if(GetDlgItemText(hDlg, IDC_COMBO_PROCESS, szTargetProcess, MAX_PATH)
				&& GetDlgItemText(hDlg, IDC_EDIT_DLL, szDLLToInject, MAX_PATH))
			{
				nPID = atoi(szTargetProcess);
				Eject(nPID, szDLLToInject);
			}
			return TRUE;

		case IDC_BUTTON_KILL:

			if(GetDlgItemText(hDlg, IDC_COMBO_PROCESS, szTargetProcess, 1024))
			{
				DWORD dwProcessID = atoi(szTargetProcess);
				if(dwProcessID != 0)
				{
					HANDLE hProcess;
					hProcess = OpenProcess (PROCESS_TERMINATE, FALSE, dwProcessID);
					TerminateProcess (hProcess,0);
					CloseHandle(hProcess);
				}
			}
			return TRUE;

		case IDC_BUTTON_ABOUT:
			DialogBox( hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hDlg, (DLGPROC)About );
			break;
			
		case IDCANCEL:

			EndDialog(hDlg, LOWORD(wParam));

			return TRUE;

		}
	}
	return FALSE;
}


LRESULT CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
