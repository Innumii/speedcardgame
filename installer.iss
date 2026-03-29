#ifndef MyAppVersion
#define MyAppVersion "0.0.0-local"
#endif

[Setup]
AppId={{6A9F579C-C9BC-4DF1-BB4B-7D299A0A9E6D}
AppName=Archicast
AppVersion={#MyAppVersion}
AppPublisher=fyl Studios
DefaultDirName={autopf}\Archicast
DefaultGroupName=Archicast
OutputDir=Output
OutputBaseFilename=MyGameSetup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "GameBuild\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Archicast"; Filename: "{app}\MyGame.exe"
Name: "{autodesktop}\Archicast"; Filename: "{app}\MyGame.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Run]
Filename: "{app}\MyGame.exe"; Description: "Launch Archicast"; Flags: nowait postinstall skipifsilent