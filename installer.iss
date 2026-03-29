#ifndef MyAppVersion
#define MyAppVersion "0.0.0-local"
#endif

[Setup]
AppId={{6A9F579C-C9BC-4DF1-BB4B-7D299A0A9E6D}
AppName=Archcast
AppVersion={#MyAppVersion}
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

[Icons]
Name: "{group}\Archcast"; Filename: "{app}\MyGame.exe"
Name: "{autodesktop}\Archcast"; Filename: "{app}\MyGame.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Run]
Filename: "{app}\MyGame.exe"; Description: "Launch Archcast"; Flags: nowait postinstall skipifsilent