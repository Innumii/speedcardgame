#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif

[Setup]
AppId={{6A9F579C-C9BC-4DF1-BB4B-7D299A0A9E6D}
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
Source: "GameBuild\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "client\assets\images\logo.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Archcast"; Filename: "{app}\MyGame.exe"; IconFilename: "{app}\logo.ico"; IconIndex: 0
Name: "{autodesktop}\Archcast"; Filename: "{app}\MyGame.exe"; IconFilename: "{app}\logo.ico"; IconIndex: 0

[Run]
Filename: "{app}\MyGame.exe"; Description: "Launch Archcast"; Flags: nowait postinstall skipifsilent