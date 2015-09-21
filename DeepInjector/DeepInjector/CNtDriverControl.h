#ifndef __NT_DRIVER_CONTROL
 #define __NT_DRIVER_CONTROL

#ifndef STRICT
 #define STRICT
#endif //STRICT

//#ifndef UNICODE
// #define UNICODE 1
//#endif

//#ifndef _UNICODE
// #define _UNICODE 1
//#endif

#include <windows.h>
#include <tchar.h>
#include <string.h>
#include <stdlib.h>
#include <crtdbg.h>



class CNtDriverControl;
//**********************************************************************//

class CNtDriverControl
{
  public:
	typedef struct
	{
		  DWORD OpCode;
		  DWORD Par1;
		  DWORD Par2;
		  DWORD Par3;
		  DWORD Par4;
		  DWORD Par5;
	} TDirectInfo,* PDirectInfo;


   explicit 
   CNtDriverControl();
   virtual ~CNtDriverControl();

   virtual
   bool       Open(const TCHAR* pszDriverFileName = NULL, const TCHAR* pszDriverName= NULL);
   virtual
   bool       Close();
   virtual
   bool       OpenDevice(const TCHAR *pszDeviceName);
   virtual
   bool       CloseDevice();
   virtual
   bool       Start();
   virtual
   bool       Stop();
   virtual
   bool       Install();
   virtual
   bool       Remove();
   virtual
   bool       Load(const TCHAR *pszDeviceName);
   virtual
   bool       Unload();
   virtual
   bool       SendCommand(DWORD OpCode, DWORD Par1, DWORD Par2, DWORD Par3, DWORD Par4 , DWORD Par5, void *Result, DWORD ResultSize);

   bool       IsWindowsNt();
   HANDLE         m_hDrv;

  protected:
   void       DriverErrorMessage();

  private:
   CNtDriverControl(const CNtDriverControl&);
   CNtDriverControl& operator=(const CNtDriverControl& right);

  protected:
   TCHAR*         m_pszDriverFileName;
   TCHAR*         m_pszDriverName;

};

void __inline CNtDriverControl::DriverErrorMessage()
{
  TCHAR *MsgBuf;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                GetLastError(),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
                (LPTSTR) &MsgBuf,
                0,
                NULL
               );
  MessageBox(GetForegroundWindow(), MsgBuf, _T("Error"), MB_OK);
  LocalFree(MsgBuf);
}

#ifndef _DebugMessage
 #ifdef _DEBUG
 #define  _DebugMessage(FunctionName)   { _CrtDbgReport(0, __FILE__, __LINE__, NULL, #FunctionName); }
 #else
 #define  _DebugMessage(FunctionName)   { ; }
 #endif
#endif //_DebugMessage(FunctionName)

#endif