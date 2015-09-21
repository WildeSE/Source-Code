// The most important part of this code was release by Kayaker
// I just added some routines to be able to use it...
// Orkblutt

#include <stdio.h>
#include <stdlib.h>
#include <ntifs.h>

#define IOPM_SIZE 0x2000
#define DIRECTNT_TYPE               40000
#define IOCTL_DIRECTNT_CONTROL      CTL_CODE(DIRECTNT_TYPE, 0x0800, METHOD_BUFFERED, FILE_READ_ACCESS)

#define OP_INJECT	1
#define OP_EJECT	2


typedef struct
{
    ULONG OpCode;
    ULONG Par1;
    ULONG Par2;
    ULONG Par3;
    ULONG Par4;
    ULONG Par5;
} TDirectNTInfo,* PDirectNTInfo;

typedef UCHAR IOPMTYP[IOPM_SIZE];
typedef struct {
    ULONG Dummy;
    IOPMTYP iopm;
} TLocalDevInfo,* PLocalDevInfo;



enum  	_KAPC_ENVIRONMENT { OriginalApcEnvironment, AttachedApcEnvironment, CurrentApcEnvironment  }KAPC_ENVIRONMENT;
///////////////////////////////////////////////////////

void KeInitializeApc(struct _KAPC *Apc, PKTHREAD thread,
                     unsigned char state_index,
                     PKKERNEL_ROUTINE ker_routine,
                     PKRUNDOWN_ROUTINE rd_routine,
                     PKNORMAL_ROUTINE nor_routine,
                     unsigned char mode,
                     void *context);
                            

void KeInsertQueueApc(struct _KAPC *APC,
                      void *SysArg1,
                      void *SysArg2,
                      unsigned char arg4);

NTSTATUS InjectDllByAPC(ULONG TargetPid, ULONG TargetTid, PUNICODE_STRING usDllPath, ULONG LdrMethodAddress);

void APCMdlCode(PVOID lpLdrLoadDll,        // pNormalContext,
                PVOID pwsDllPath,    // pSysArg1,
                PVOID pusLengthInfo);// pSysArg2

void APCMdlCodeEnd();

//PVOID GetCallAddress( PCSTR Name );


///////////////////////////////////////////////////////


PKAPC pKAPC;

// for code MDL

PMDL pMDLApcCode = NULL;
PVOID *pMappedCode;


// for data MDL

#define MAX_PATH 260

PMDL pMDLApcData = NULL;
PVOID *pMappedData;


// These two variables define a UNICODE representation of
// our dll path

// Allocate space for data
// Can't use ExFreePool else it unmaps (overwrites first dword)
// of both kernel and user(MDL) memory mappings

ULONG unicodeLengthInfo;
char pAPCData[MAX_PATH + 4];


void
APCKernelRoutine(
                IN PKAPC            pKAPC,
                IN PKNORMAL_ROUTINE pUserAPC,
                IN PVOID            pContext,
                IN PVOID            pSysArg1,
                IN PVOID            pSysArg2
               )
{

    DbgPrint("APCKernelRoutine Entered\n");

    ExFreePool(pKAPC);

    return;
}


/***********************************************************

                InjectDllByAPC

    In order to inject a dll into a process before it loads,
    we schedule an APC which will be triggered as soon as
    execution returns to user mode.    The APC contains code
    which will call LdrLoadDll with the name of our dll.

    We create 2 MDL's mapped into user mode from which the
    APC will execute.  One contains the executable code itself,
    the other contains the full path of the dll in wide string
    format.


***********************************************************/

NTSTATUS InjectDllByAPC(ULONG TargetPid, ULONG TargetTid, PUNICODE_STRING usDllPath, ULONG LdrMethodAddress)
{

	KAPC_STATE ApcState;
    ULONG size;
    PEPROCESS       TargetProcess;
    PKTHREAD        TargetThread;

    ULONG arg1 = 0;
    ULONG arg2 = 0;
    ULONG arg3 = 0;

    /*************************************************
                    Set up the MDL
    **************************************************/


    //    Create the executable MDL section
    // Allocate an MDL large enough to map the code to be executed
    size = (unsigned char*)APCMdlCodeEnd - (unsigned char*)APCMdlCode;

    pMDLApcCode = IoAllocateMdl (APCMdlCode, size, FALSE,FALSE,NULL);

    if (!pMDLApcCode)
        return STATUS_UNSUCCESSFUL;


    // Probe for access and lock pages

    MmProbeAndLockPages (
        pMDLApcCode,
        KernelMode,
        IoWriteAccess);


    //////////////////////////////////////////////
    //    Create the data MDL section

    RtlZeroMemory(pAPCData, sizeof( pAPCData));



    memcpy( (char*) pAPCData, usDllPath->Buffer, usDllPath->Length);
    unicodeLengthInfo = *(ULONG*) usDllPath;


    //////////////////////////////////////////////////

    // Allocate an MDL large enough to map the data we need

    pMDLApcData = IoAllocateMdl (pAPCData, sizeof(pAPCData), FALSE,FALSE,NULL);


    if (!pMDLApcData)
        return STATUS_UNSUCCESSFUL;


    // Probe for access and lock pages

    MmProbeAndLockPages (
        pMDLApcData,
        KernelMode,
        IoWriteAccess);


    //////////////////////////////////////////////
    // Get the target EPROCESS and ETHREAD objects

    PsLookupProcessByProcessId((HANDLE)TargetPid, &TargetProcess);
    PsLookupThreadByThreadId ((PVOID) TargetTid, &TargetThread);

    //////////////////////////////////////////////
    // Attach to the process

    KeStackAttachProcess (
       (PKPROCESS) &TargetProcess,
	    &ApcState);


    //////////////////////////////////////////////
    // Map the code MDL pages into the specified process

    pMappedCode = (PVOID*) MmMapLockedPagesSpecifyCache (
                      pMDLApcCode,
                      UserMode,
                      MmCached,
                      NULL,
                      FALSE,
                      NormalPagePriority);



    // Map the data MDL pages into the specified process

    pMappedData = (PVOID*) MmMapLockedPagesSpecifyCache (
                      pMDLApcData,
                      UserMode,
                      MmCached,
                      NULL,
                      FALSE,
                      NormalPagePriority);


    //////////////////////////////////////////////
    //Detach from the process


    KeUnstackDetachProcess (&ApcState);


    //////////////////////////////////////////////
    /*************************************************
                    Set up the APC
    **************************************************/
    // Set the arguments we will pass to the APC

    arg1 = (ULONG) LdrMethodAddress;		//LdrLoadDll or LdrUnloadDll;
    arg2 = (ULONG) pMappedData;				// dll path in wide string format
    arg3 = (ULONG) unicodeLengthInfo;		// UNICODE_STRING Length and MaximumLength



    // Allocate space for KAPC object

    pKAPC = (PKAPC) ExAllocatePool( NonPagedPool, sizeof(KAPC) );

    RtlZeroMemory(pKAPC, sizeof(KAPC));


    // Initialize the user-mode APC

    KeInitializeApc(
        pKAPC,
        TargetThread,
        OriginalApcEnvironment,
        (PKKERNEL_ROUTINE)APCKernelRoutine,
        NULL,
        (PKNORMAL_ROUTINE) pMappedCode,    // MDL address in user context
        UserMode,
        (PVOID)arg1        // Context (first parameter passed to normal APC)
    );



    // Insert it to the queue of the target thread

    KeInsertQueueApc(pKAPC,
                 (PVOID)arg2,                // Context 2.
                 (PVOID)arg3,                // Context 3.
                 0  );                       // Priority increment
            

    /* Mark the thread as alertable to force it to deliver
    the APC on the next return to the user-mode.

    Set KAPC_STATE.UserApcPending on.
    */


    *((unsigned char *)TargetThread + 0x4a) = 1;



    /*************************************************
                    Cleanup
    **************************************************/


    // Unlock and Free the MDL's

    if (pMDLApcCode)
    {
        // don't unmap
        //MmUnmapLockedPages(pMappedCode, pMDLApcCode);

        MmUnlockPages(pMDLApcCode);
        IoFreeMdl(pMDLApcCode);
    }



    if (pMDLApcData)
    {
        MmUnlockPages(pMDLApcData);
        IoFreeMdl(pMDLApcData);
    }


    //////////////////////////////////////////////

    // Dereference the EPROCESS and ETHREAD objects

    ObDereferenceObject(TargetProcess);
    ObDereferenceObject(TargetThread);


    return STATUS_SUCCESS;


}


/***********************************************************

            APCMdlCode

    MDL APC routine for a user-mode APC

    pLdrLoadDll(NULL, &DllCharacteristics, &usDllName, &DllHandle);


***********************************************************/

void
APCMdlCode(
    IN PVOID            lpLdrLoadDll,       // pNormalContext,
    IN PVOID            pwsDllPath,         // pSysArg1,
    IN PVOID            pwsDllPathLength    // pSysArg2
)
{

	
    UNICODE_STRING usDllName;
    ULONG DllCharacteristics = 0;
    PVOID DllHandle = 0;            // Returns base address of dll
	
    usDllName.Length = (USHORT) pwsDllPathLength;
    usDllName.MaximumLength = usDllName.Length + 2;
    usDllName.Buffer = (USHORT*) pwsDllPath;

    __asm
    {
        pushad

        lea eax, DllHandle
        push eax
        lea eax, usDllName
        push eax
        lea eax, DllCharacteristics
        push eax
        push 0

        call [lpLdrLoadDll]

		nop
		nop

        popad

    }
}


/***********************************************************
            APCMdlCodeEnd
  Just a reference to calculate size of the above APC routine
***********************************************************/

void APCMdlCodeEnd()
{

}

/***********************************************************/


NTSTATUS IoControl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
    PIO_STACK_LOCATION pisl;
    NTSTATUS ns = STATUS_SUCCESS;
    ULONG BuffSize;
    PVOID pBuff;
    ULONG InBufSize;
    PDirectNTInfo InBuf;
	ANSI_STRING anTemp;
	UNICODE_STRING ucTemp;

	pisl = IoGetCurrentIrpStackLocation (Irp);

    BuffSize = pisl->Parameters.DeviceIoControl.OutputBufferLength;

    pBuff = Irp->AssociatedIrp.SystemBuffer;

    Irp->IoStatus.Information = 0;

    if (pisl->MajorFunction == IRP_MJ_DEVICE_CONTROL)
    {
        InBuf = (PDirectNTInfo) Irp->AssociatedIrp.SystemBuffer;

        switch (InBuf->OpCode)
        {
			case OP_INJECT:
				RtlInitAnsiString(&anTemp, (char*)InBuf->Par3);
				ns = RtlAnsiStringToUnicodeString(&ucTemp, &anTemp, TRUE);
				if(NT_SUCCESS(ns))
				{
					ns = InjectDllByAPC((ULONG)InBuf->Par1, (ULONG)InBuf->Par2, &ucTemp, InBuf->Par4);
					RtlFreeUnicodeString(&ucTemp);
				}
				break;

			case OP_EJECT:
				// to implement... we just need the module handle and call LdrUnloadDll.
				break;
			default:
				ns = STATUS_INVALID_DEVICE_REQUEST;
		}
	}

	Irp->IoStatus.Status = ns;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);

	return ns;

}


VOID DriverUnload( IN PDRIVER_OBJECT DriverObject )
{

    UNICODE_STRING ucDosDevices;
    RtlInitUnicodeString(&ucDosDevices, L"\\DosDevices\\inject");
   
    IoDeleteSymbolicLink(&ucDosDevices);
    IoDeleteDevice(DriverObject->DeviceObject);
}


NTSTATUS DriverEntry( IN PDRIVER_OBJECT theDriverObject, IN PUNICODE_STRING theRegistryPath )
{

	NTSTATUS ntStatus;
	int i;
	UNICODE_STRING ucDeviceINJECT;
	UNICODE_STRING ucDosDeviceINJECT;


	theDriverObject->DriverUnload  = DriverUnload; 

	for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        theDriverObject->MajorFunction[i] = IoControl;

    theDriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = IoControl;



	RtlInitUnicodeString(&ucDeviceINJECT, L"\\Device\\inject");

	ntStatus = IoCreateDevice (theDriverObject
                                , sizeof(TLocalDevInfo)
                                , &ucDeviceINJECT
                                , DIRECTNT_TYPE
                                , 0
                                , FALSE
                                , &theDriverObject->DeviceObject);

    if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("CreateDevice fails\n");
        return ntStatus;
	}

	RtlInitUnicodeString(&ucDosDeviceINJECT, L"\\DosDevices\\inject");

    ntStatus = IoCreateSymbolicLink(&ucDosDeviceINJECT, &ucDeviceINJECT);
    if (!NT_SUCCESS(ntStatus))
	{
		DbgPrint("CreateSymbolicLink fails\n");
        IoDeleteDevice(theDriverObject->DeviceObject);
	}

	return ntStatus;
}
