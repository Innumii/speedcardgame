#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
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
OutputBaseFilename=ArchcastSetup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64

[Files]
; -------------------------
; Include GameBuild contents
; -------------------------
Source: "GameBuild\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Archcast"; Filename: "{app}\MyGame.exe"; IconIndex: 0; AppUserModelID: "Archcast"
Name: "{autodesktop}\Archcast"; Filename: "{app}\MyGame.exe"; IconIndex: 0; AppUserModelID: "Archcast"

[Run]
; -------------------------
; Launch app post-install
; -------------------------
Filename: "{app}\MyGame.exe"; Description: "Launch Archcast"; Flags: nowait postinstall skipifsilent