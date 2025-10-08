Name "OBS Websockets Relay"
OutFile "obs-ws-relay-setup.exe"
InstallDir "C:\ProgramData\obs-studio\plugins\obs-ws-relay"

!define REG_UNINSTALL "Software\Microsoft\Windows\CurrentVersion\Uninstall\OBSWebsocketsRelay"

Page directory
Page instfiles

Section "Install"
    SetOutPath "$INSTDIR\bin\64bit"
    File "build_x64\Release\obs-ws-relay.dll"
    File "build_x64\Release\libcrypto-3-x64.dll"
    File "build_x64\Release\libssl-3-x64.dll"
    File "build_x64\Release\uv.dll"
    File "build_x64\Release\websockets.dll"
    File "build_x64\Release\zlib1.dll"

    SetOutPath "$INSTDIR\data\locale"
    File "data\locale\en-US.ini"

    WriteUninstaller "$INSTDIR\Uninstall.exe"

    WriteRegStr HKLM "${REG_UNINSTALL}" "DisplayName" "OBS Websockets Relay"
    WriteRegStr HKLM "${REG_UNINSTALL}" "UninstallString" "$INSTDIR\Uninstall.exe"
    WriteRegStr HKLM "${REG_UNINSTALL}" "DisplayVersion" "1.0.1"
    WriteRegStr HKLM "${REG_UNINSTALL}" "Publisher" "BlueGlassBlock"
    WriteRegStr HKLM "${REG_UNINSTALL}" "InstallLocation" "$INSTDIR"
    WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoModify" 1
    WriteRegDWORD HKLM "${REG_UNINSTALL}" "NoRepair" 1
SectionEnd

Section "Uninstall"
    RMDir /r "$INSTDIR"

    DeleteRegKey HKLM "${REG_UNINSTALL}"
SectionEnd
