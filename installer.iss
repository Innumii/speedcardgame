#ifndef MyAppVersion
#define MyAppVersion "0.0.0-local"
#endif

[Setup]
AppId={{6A9F579C-C9BC-4DF1-BB4B-7D299A0A9E6D}
AppName=My Game
AppVersion={#MyAppVersion}
AppPublisher=My Game Studio
DefaultDirName={autopf}\My Game
DefaultGroupName=My Game
OutputDir=Output
OutputBaseFilename=MyGameSetup
Compression=lzma
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "GameBuild\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Source: "{group}\My Game"; Target: "{app}\MyGame.exe"
Source: "{autodesktop}\My Game"; Target: "{app}\MyGame.exe"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional icons:"; Flags: unchecked

[Run]
Filename: "{app}\MyGame.exe"; Description: "Launch My Game"; Flags: nowait postinstall skipifsilent