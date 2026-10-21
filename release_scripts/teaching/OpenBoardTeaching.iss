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
Source: "..\..\dist\vc_redist.x64.exe"; Flags: dontcopy
Source: "{#AppRoot}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\OpenBoard Teaching"; Filename: "{app}\OpenBoardTeaching.exe"
Name: "{autodesktop}\OpenBoard Teaching"; Filename: "{app}\OpenBoardTeaching.exe"; Tasks: desktopicon
[Run]
Filename: "{app}\OpenBoardTeaching.exe"; Description: "Open OpenBoard Teaching"; Flags: nowait postinstall skipifsilent unchecked

[Code]
function NextVersionPart(var Value: String): Integer;
var
  Dot: Integer;
  Part: String;
begin
  Dot := Pos('.', Value);
  if Dot = 0 then begin
    Part := Value;
    Value := '';
  end else begin
    Part := Copy(Value, 1, Dot - 1);
    Delete(Value, 1, Dot);
  end;
  Result := StrToIntDef(Part, 0);
end;

function VersionAtLeast(Actual, Required: String): Boolean;
var
  I, AV, RV: Integer;
begin
  if (Length(Actual) > 0) and ((Actual[1] = 'v') or (Actual[1] = 'V')) then
    Delete(Actual, 1, 1);
  Result := True;
  for I := 0 to 3 do begin
    AV := NextVersionPart(Actual);
    RV := NextVersionPart(Required);
    if AV <> RV then begin
      Result := AV > RV;
      Exit;
    end;
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  InstalledVersion: String;
  ExitCode: Integer;
begin
  Result := '';
  if RegQueryStringValue(HKLM64, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64', 'Version', InstalledVersion) then
    if VersionAtLeast(InstalledVersion, '{#GetVersionNumbersString("..\..\dist\vc_redist.x64.exe")}') then Exit;
  ExtractTemporaryFile('vc_redist.x64.exe');
  if not ShellExec('runas', ExpandConstant('{tmp}\vc_redist.x64.exe'), '/install /quiet /norestart', '', SW_HIDE, ewWaitUntilTerminated, ExitCode) then begin
    Result := 'The Microsoft Visual C++ runtime is required. Its installation needs administrator approval.';
    Exit;
  end;
  if (ExitCode <> 0) and (ExitCode <> 3010) and (ExitCode <> 1638) then
    Result := 'Microsoft Visual C++ runtime installation failed (' + IntToStr(ExitCode) + ').';
  if ExitCode = 3010 then NeedsRestart := True;
end;
