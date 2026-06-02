#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

#ifndef GameBuildDir
  #define GameBuildDir "GameBuild"
#endif

#ifndef OutputBaseFilename
  #define OutputBaseFilename "ArchcastSetup"
#endif

[Setup]
; -------------------------
; Basic app info
; -------------------------
AppId={{6A9F579C-C9BC-4DF1-BB4B-7D299A0A9E6D}}
AppName=Archcast
AppVersion={#MyAppVersion}
AppVerName=Archcast
AppPublisher=FYL Studios
DefaultDirName={autopf}\Archcast
DefaultGroupName=Archcast
OutputDir=Output
OutputBaseFilename={#OutputBaseFilename}
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64
SetupIconFile=client\assets\images\logo.ico
UninstallDisplayIcon={app}\Archcast.exe

[Files]
; -------------------------
; Include GameBuild contents
; -------------------------
Source: "{#GameBuildDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Archcast"; Filename: "{app}\Archcast.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Archcast.exe"
Name: "{autodesktop}\Archcast"; Filename: "{app}\Archcast.exe"; WorkingDir: "{app}"; IconFilename: "{app}\Archcast.exe"

[Run]
; -------------------------
; Launch app post-install
; -------------------------
Filename: "{app}\Archcast.exe"; Description: "Launch Archcast"; Flags: nowait postinstall skipifsilent