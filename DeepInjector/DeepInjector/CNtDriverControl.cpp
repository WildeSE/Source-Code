#define STRICT
#include "stdafx.h"
#include "CNtDriverControl.h"
#include <WinSvc.h>
#include <WinIoCtl.h>
#include <winnt.h>


#define DIRECT_TYPE               40000
#define IOCTL_DIRECT_CONTROL      CTL_CODE(DIRECT_TYPE,0x0800,METHOD_BUFFERED,FILE_READ_ACCESS)

CNtDriverControl::CNtDriverControl()
{
	m_hDrv = INVALID_HANDLE_VALUE;
	m_pszDriverFileName = NULL;
	m_pszDriverName = NULL;
}

CNtDriverControl::~CNtDriverControl(void)
{
	Close();
}



bool CNtDriverControl::SendCommand(DWORD OpCode, DWORD Par1, DWORD Par2, DWORD Par3, DWORD Par4 , DWORD Par5, void *Result, DWORD ResultSize)
{

	TDirectInfo I = {OpCode,Par1,Par2,Par3,Par4,Par5};
	DWORD ResultLen;
	if (m_hDrv == INVALID_HANDLE_VALUE )
	{
		return false;
	}

	if (!DeviceIoControl(m_hDrv,DWORD(IOCTL_DIRECT_CONTROL), &I, sizeof(I), Result, ResultSize, &ResultLen, NULL))
	{
		//DriverErrorMessage();
		return false;
	}
	


	return true;
}

bool CNtDriverControl::IsWindowsNt()
{
	OSVERSIONINFOEX  OSVer;

	OSVer.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	OSVer.dwOSVersionInfoSize = sizeof(OSVer);

	if (!GetVersionExA((OSVERSIONINFOA *)&OSVer))
		return false;

	switch(OSVer.dwPlatformId << 16 | OSVer.dwMajorVersion << 8 | OSVer.dwMinorVersion)
	{

	  case VER_PLATFORM_WIN32_NT     <<16|0x0500| 1:
		  if(!GetSystemMetrics(87))
			return TRUE;
		  
	  default:
		  MessageBox (0
			  , _T("DeepMonitor designed only for Windows XP !")
			  , _T("Information")
			  , MB_OK);
		  return false;
	
	}
	
	return false;
	
}

bool CNtDriverControl::Open(const TCHAR* pszDriverFileName, const TCHAR* pszDriverName)
{

	if (IsWindowsNt() == false)
		return false;

	if (!Close())
		return false;

	int Length;

	if (pszDriverFileName != NULL)
	{
		Length = lstrlen(pszDriverFileName);
		if (Length != 0)
		{
			m_pszDriverFileName = new TCHAR[Length+sizeof(TCHAR)];
			if (m_pszDriverFileName == NULL)  
				return false;
			lstrcpy(m_pszDriverFileName, pszDriverFileName);
		}
	}

	if (pszDriverName != NULL)
	{
		Length = lstrlen(pszDriverName);
		if (Length != 0)
		{
			m_pszDriverName = new TCHAR[Length+sizeof(TCHAR)];
			if (m_pszDriverName == NULL)  
			{
				if (m_pszDriverFileName != NULL)
					delete[] m_pszDriverFileName;
				m_pszDriverFileName = NULL;
				return false;
			}
			lstrcpy(m_pszDriverName, pszDriverName);
		}
	}

	return true;
}

bool CNtDriverControl::Close()
{

	CloseDevice();

	if (m_pszDriverFileName)
		delete[] m_pszDriverFileName;
	m_pszDriverFileName = NULL;
	if (m_pszDriverName)
		delete[] m_pszDriverName;
	m_pszDriverName = NULL;

	return true;
}

bool CNtDriverControl::OpenDevice(const TCHAR *pszDeviceName)
{

	if (pszDeviceName == NULL)
		return false;

	HANDLE   hDevice;
	

	hDevice = CreateFile(_T(pszDeviceName),
		GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if ( hDevice == ((HANDLE)INVALID_HANDLE_VALUE))
		return false;

	// If user wants handle, give it to them.  Otherwise, just close it.
	m_hDrv = hDevice;

	return true;
}

bool CNtDriverControl::CloseDevice()
{

	if (m_hDrv != INVALID_HANDLE_VALUE)
		CloseHandle(m_hDrv);

	m_hDrv = INVALID_HANDLE_VALUE;

	return true;
}

bool CNtDriverControl::Start()
{

	if (m_pszDriverName == NULL)
		return false;

	SC_HANDLE  schService;
	bool       ret;
	SC_HANDLE  schSCManager;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL)
		return false;

	schService = OpenService(schSCManager,
		m_pszDriverName,
		SERVICE_ALL_ACCESS);
	if (schService == NULL)
	{
		CloseServiceHandle(schSCManager);
		return false;
	}

	ret =    (StartService(schService, 0, NULL) == TRUE)
		|| (GetLastError() == ERROR_SERVICE_ALREADY_RUNNING);

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return ret;
}

bool CNtDriverControl::Stop()
{

	if (m_pszDriverName == NULL)
		return false;

	SC_HANDLE       schService;
	bool            ret;
	SERVICE_STATUS  serviceStatus;
	SC_HANDLE       schSCManager;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL)
		return false;

	schService = OpenService(schSCManager, m_pszDriverName, SERVICE_ALL_ACCESS);
	if (schService == NULL)
	{
		CloseServiceHandle(schSCManager);
		return false;
	}

	ret = ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus) == TRUE;

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return ret;
}

typedef ULONG NTSTATUS;
typedef NTSTATUS (NTAPI *RtlDecompressBuffer_t)(ULONG , PVOID , ULONG , PVOID , ULONG , PULONG); 
RtlDecompressBuffer_t pRtlDecompressBuffer;

/****************************************************************************
*
*    FUNCTION: Install(IN SC_HANDLE)
*
*    PURPOSE: Creates a driver service.
*
****************************************************************************/
bool CNtDriverControl::Install()
{

	if (m_pszDriverName == NULL || m_pszDriverFileName == NULL)
		return false;

	SC_HANDLE       schSCManager;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL)
		return false;

	SC_HANDLE  schService;




	schService = CreateService(schSCManager,          // SCManager database
		m_pszDriverName,       // name of service
		m_pszDriverName,       // name to display
		SERVICE_ALL_ACCESS,    // desired access
		SERVICE_KERNEL_DRIVER, // service type
		SERVICE_DEMAND_START,  // start type
		SERVICE_ERROR_NORMAL,  // error control type
		m_pszDriverFileName,   // service's binary
		NULL,                  // no load ordering group
		NULL,                  // no tag identifier
		NULL,                  // no dependencies
		NULL,                  // LocalSystem account
		NULL                   // no password
		);

	if (schService == NULL)
	{
		CloseServiceHandle(schSCManager);
		return false;
	}

	CloseServiceHandle(schService);

	CloseServiceHandle(schSCManager);


	return true;
}

/****************************************************************************
*
*    FUNCTION: Remove(IN SC_HANDLE)
*
*    PURPOSE: Deletes the driver service.
*
****************************************************************************/
bool CNtDriverControl::Remove()
{

	if (m_pszDriverName == NULL)
		return false;

	SC_HANDLE       schSCManager;

	schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
	if (schSCManager == NULL)
		return false;

	SC_HANDLE  schService;
	bool       ret;

	schService = OpenService(schSCManager,
		m_pszDriverName,
		SERVICE_ALL_ACCESS);

	if (schService == NULL)
	{
		CloseServiceHandle(schSCManager);
		return false;
	}

	ret = DeleteService(schService) == TRUE;

	CloseServiceHandle(schService);
	CloseServiceHandle(schSCManager);

	return ret;
}


/****************************************************************************
*
*    FUNCTION: Unload()
*
*    PURPOSE: Stops the driver and has the configuration manager unload it.
*
****************************************************************************/
bool CNtDriverControl::Unload()
{
	CloseDevice();
	Stop();
	Remove();

	return true;
}



/****************************************************************************
*
*    FUNCTION: Load()
*
*    PURPOSE: Registers a driver with the system configuration manager 
*        and then loads it.
*
****************************************************************************/
bool CNtDriverControl::Load(const TCHAR *pszDeviceName)
{
	bool            okay;

	// Remove previous instance
	Remove();

	// Ignore success of installation: it may already be installed.
	Install();

	// Ignore success of start: it may already be started.
	Start();

	// Do make sure we can open it.
	okay = OpenDevice(pszDeviceName);

	return okay;
}
