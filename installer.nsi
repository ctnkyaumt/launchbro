; launchbro installer
; NSIS script

!include "MUI2.nsh"
!include "LogicLib.nsh"

!define APP_NAME "launchbro"
!define APP_VERSION "2.9.15"

Name "${APP_NAME} ${APP_VERSION}"
OutFile "launchbro-setup.exe"
; Per-user install under Documents, not Program Files: launchbro.exe runs asInvoker (see
; res/manifest.xml) and self-updates its own install folder (browser downloads/installs,
; self-update) without ever elevating. Program Files requires admin to write to, which broke
; every self-update after a Program-Files install - Documents is always writable by the owning
; user, so no admin/UAC is needed to install OR to keep updating afterward.
InstallDir "$DOCUMENTS\${APP_NAME}"
RequestExecutionLevel user

; Pages
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

; Languages
!insertmacro MUI_LANGUAGE "English"

Section "Install"
    SetOutPath "$INSTDIR"

    ; Copy binaries
    File /r "dist\launchbro\*"

    ; Create uninstaller
    WriteUninstaller "$INSTDIR\uninstall.exe"

    ; Create start menu shortcuts
    CreateDirectory "$SMPROGRAMS\${APP_NAME}"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk" "$INSTDIR\64\launchbro.exe"
    CreateShortcut "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe"

    ; Registry entries (HKCU: this is a per-user install, HKLM would need admin)
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayName" "${APP_NAME}"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayIcon" "$INSTDIR\64\launchbro.exe"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "Publisher" "ctnkyaumt"
    WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}" "DisplayVersion" "${APP_VERSION}"
SectionEnd

Section "Uninstall"
    ; Clean up system changes before killing processes
    ${If} ${FileExists} "$INSTDIR\64\launchbro.exe"
        ExecWait '"$INSTDIR\64\launchbro.exe" /cleanup'
    ${ElseIf} ${FileExists} "$INSTDIR\32\launchbro.exe"
        ExecWait '"$INSTDIR\32\launchbro.exe" /cleanup'
    ${EndIf}

    ; Close launchbro / chrlauncher and bundled browser processes from this install root
    ; (hidden window, broad error suppression - this used to flash a visible PowerShell
    ; console with red WMI/CIM error text since only Stop-Process had -ErrorAction set)
    ExecWait "$\"$SYSDIR\WindowsPowerShell\v1.0\powershell.exe$\" -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -Command $\"$\$ErrorActionPreference=$\'SilentlyContinue$\';$\$roots=@($\'$INSTDIR$\');$\$legacy=Join-Path ([System.IO.Path]::GetDirectoryName($\'$INSTDIR$\')) $\'chrlauncher$\';if(Test-Path $\$legacy){$\$roots+=$\$legacy};Get-CimInstance Win32_Process | Where-Object { $\$path=$\$_.ExecutablePath; $\$path -and @($\$roots | Where-Object { $\$path.StartsWith($\$_, [System.StringComparison]::OrdinalIgnoreCase) }).Count -gt 0 } | ForEach-Object { Stop-Process -Id $\$_.ProcessId -Force -ErrorAction SilentlyContinue }$\""

    ; Remove shortcuts
    Delete "$SMPROGRAMS\${APP_NAME}\${APP_NAME}.lnk"
    Delete "$SMPROGRAMS\${APP_NAME}\Uninstall.lnk"
    RMDir "$SMPROGRAMS\${APP_NAME}"

    ; Ask about profiles
    MessageBox MB_YESNO|MB_ICONQUESTION "Do you want to keep your browser profiles?" IDYES KeepProfiles

    ; Delete only what launchbro owns
    RMDir /r "$INSTDIR\32"
    RMDir /r "$INSTDIR\64"
    RMDir /r "$INSTDIR\i18n"
    RMDir /r "$INSTDIR\_launchbro_new"
    Delete "$INSTDIR\launchbro.ini"
    Delete "$INSTDIR\launchbro.lng"
    Delete "$INSTDIR\History.txt"
    Delete "$INSTDIR\License.txt"
    Delete "$INSTDIR\Readme.txt"
    Delete "$INSTDIR\RegistryCleaner.reg"
    Delete "$INSTDIR\SetDefaultBrowser.bat"
    Delete "$INSTDIR\_launchbro_update.bat"
    Delete "$INSTDIR\uninstall.exe"
    RMDir "$INSTDIR"
    Goto CleanupRegistry

KeepProfiles:
    ; Delete only app files, keep bin and profile folders
    Delete "$INSTDIR\64\launchbro.exe"
    Delete "$INSTDIR\32\launchbro.exe"
    Delete "$INSTDIR\64\*.dll"
    Delete "$INSTDIR\32\*.dll"
    Delete "$INSTDIR\launchbro.ini"
    Delete "$INSTDIR\launchbro.lng"
    Delete "$INSTDIR\History.txt"
    Delete "$INSTDIR\License.txt"
    Delete "$INSTDIR\Readme.txt"
    Delete "$INSTDIR\RegistryCleaner.reg"
    Delete "$INSTDIR\SetDefaultBrowser.bat"
    Delete "$INSTDIR\_launchbro_update.bat"
    Delete "$INSTDIR\uninstall.exe"
    RMDir /r "$INSTDIR\i18n"
    RMDir /r "$INSTDIR\_launchbro_new"
    ; bin and profile folders remain

CleanupRegistry:
    DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APP_NAME}"
SectionEnd
