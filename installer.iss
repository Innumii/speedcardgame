#ifndef MyAppVersion
#define MyAppVersion "1.0.0"
#endif

[Setup]
; -------------------------
; Basic app info
; -------------------------
AppId={{6A9F579C-C9BC-4DF1-BB4B-7D299A0A9E6D}
AppName=Archcast
AppVersion={#MyAppVersion}
AppPublisher=FYL Studios
DefaultDirName={autopf}\Archcast
DefaultGroupName=Archcast

; -------------------------
; Output / compression
; -------------------------
OutputDir=Output
OutputBaseFilename=ArchcastSetup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64

; -------------------------
; Files to include
; -------------------------
[Files]
; Recursively include all game build files
Source: "GameBuild\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

; -------------------------
; Icons (shortcuts)
; -------------------------
[Icons]
; Start Menu shortcut — explicitly reference embedded icon
Name: "{group}\Archcast"; Filename: "{app}\MyGame.exe"; IconFilename: "{app}\MyGame.exe"; IconIndex: 0

; Desktop shortcut — optional, unchecked by default
Name: "{autodesktop}\Archcast"; Filename: "{app}\MyGame.exe"; IconFilename: "{app}\MyGame.exe"; IconIndex: 0; Tasks: desktopicon

; -------------------------
; Optional tasks
; -------------------------
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

; -------------------------
; Run after install
; -------------------------
[Run]
Filename: "{app}\MyGame.exe"; Description: "Launch Archcast"; Flags: nowait postinstall skipifsilent