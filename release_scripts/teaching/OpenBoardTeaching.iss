#define AppRoot "..\..\dist\app"
#define AppVersion "1.7.7.1"
[Setup]
AppId={{2269D566-C066-4763-8D2D-48EE7AE1C83C}
AppName=OpenBoard Teaching
AppVersion={#AppVersion}
AppPublisher=OpenBoard Teaching fork
DefaultDirName={localappdata}\Programs\OpenBoardTeaching
DefaultGroupName=OpenBoard Teaching
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..\..\dist
OutputBaseFilename=OpenBoard-Teaching-Windows-x64-Setup
SetupIconFile=..\..\resources\win\OpenBoard.ico
LicenseFile=..\..\LICENSE
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\OpenBoardTeaching.exe
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; Flags: unchecked
[Files]
Source: "{#AppRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\OpenBoard Teaching"; Filename: "{app}\OpenBoardTeaching.exe"
Name: "{autodesktop}\OpenBoard Teaching"; Filename: "{app}\OpenBoardTeaching.exe"; Tasks: desktopicon
[Run]
Filename: "{app}\OpenBoardTeaching.exe"; Description: "Open OpenBoard Teaching"; Flags: nowait postinstall skipifsilent unchecked
