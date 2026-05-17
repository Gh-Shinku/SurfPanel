; -- installer.iss --

#define MyAppName "SurfPanel"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Shinku"
#define MyAppURL "https://github.com/shinku/SurfPanel"
#define MyAppExeName "SurfPanel.exe"
#define MyAppId "{{4D2F8EC0-8A04-43EF-B8C8-D8F847A16D96}}"
#define MyAppUninstallKey "Software\Microsoft\Windows\CurrentVersion\Uninstall\{4D2F8EC0-8A04-43EF-B8C8-D8F847A16D96}_is1"
#define MyProjectRoot SourcePath
#define MyMingwRoot "C:\Users\shinku\AppData\Local\msys2\mingw64"
#define MyMingwBin AddBackslash(MyMingwRoot) + "bin"
#define MyBuildDir AddBackslash(MyProjectRoot) + "build"
#define MyConfigDir AddBackslash(MyProjectRoot) + "config"
#define MySetupOutputDir AddBackslash(MyProjectRoot) + "Output"
#define MySetupIcon AddBackslash(MyProjectRoot) + "assets\SurfPanel.ico"
#define MyLicenseFile AddBackslash(MyProjectRoot) + "LICENSE"

[Setup]
AppId={#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir={#MySetupOutputDir}
OutputBaseFilename=SurfPanel_Setup
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
SetupIconFile={#MySetupIcon}
LicenseFile={#MyLicenseFile}
VersionInfoVersion={#MyAppVersion}
VersionInfoDescription={#MyAppName}
VersionInfoCompany={#MyAppPublisher}
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "Start SurfPanel with Windows"; Flags: unchecked

[Dirs]
Name: "{app}\config"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletevalue; Tasks: autostart

[Files]
; Main executable
Source: "{#MyBuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt DLLs required by SurfPanel (Widgets app)
Source: "{#MyBuildDir}\Qt6Core.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Gui.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\Qt6Widgets.dll"; DestDir: "{app}"; Flags: ignoreversion

; MinGW runtime DLLs referenced by ldd (from MSYS2 mingw64\bin)
Source: "{#MyMingwBin}\libgcc_s_seh-1.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libstdc++-6.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libfreetype-6.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libmd4c.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libharfbuzz-0.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libpng16-16.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\zlib1.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libb2-1.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libdouble-conversion.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libpcre2-16-0.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libzstd.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libbz2-1.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libbrotlidec.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libglib-2.0-0.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libgraphite2.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libbrotlicommon.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libintl-8.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libpcre2-8-0.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libiconv-2.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Minimal Qt plugin set for Qt6 Widgets
Source: "{#MyBuildDir}\platforms\qwindows.dll"; DestDir: "{app}\platforms"; Flags: ignoreversion
Source: "{#MyBuildDir}\imageformats\qico.dll"; DestDir: "{app}\imageformats"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyBuildDir}\styles\qmodernwindowsstyle.dll"; DestDir: "{app}\styles"; Flags: ignoreversion skipifsourcedoesntexist

; Ship initial config files with the app
Source: "{#MyConfigDir}\*"; DestDir: "{app}\config"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
var
  PreviousInstallDir: string;
  PreviousUninstallerPath: string;
  ConfigBackupDir: string;
  KeepExistingConfig: Boolean;

function QueryExistingInstallValue(ValueName: string; var Value: string): Boolean;
begin
  Result :=
    RegQueryStringValue(HKCU, '{#MyAppUninstallKey}', ValueName, Value) or
    RegQueryStringValue(HKLM, '{#MyAppUninstallKey}', ValueName, Value);
end;

function ExtractExecutablePath(CommandLine: string): string;
var
  EndQuotePos: Integer;
  SpacePos: Integer;
begin
  CommandLine := Trim(CommandLine);
  Result := CommandLine;

  if CommandLine = '' then
    exit;

  if Copy(CommandLine, 1, 1) = '"' then begin
    Delete(CommandLine, 1, 1);
    EndQuotePos := Pos('"', CommandLine);
    if EndQuotePos > 0 then
      Result := Copy(CommandLine, 1, EndQuotePos - 1)
    else
      Result := CommandLine;
  end else begin
    SpacePos := Pos(' ', CommandLine);
    if SpacePos > 0 then
      Result := Copy(CommandLine, 1, SpacePos - 1);
  end;
end;

function CopyDirectoryRecursive(SourceDir: string; DestDir: string): Boolean;
var
  FindRec: TFindRec;
  SourcePath: string;
  DestPath: string;
begin
  Result := True;

  if not DirExists(SourceDir) then
    exit;

  if not ForceDirectories(DestDir) then begin
    Result := False;
    exit;
  end;

  if FindFirst(AddBackslash(SourceDir) + '*', FindRec) then begin
    try
      repeat
        if (FindRec.Name <> '.') and (FindRec.Name <> '..') then begin
          SourcePath := AddBackslash(SourceDir) + FindRec.Name;
          DestPath := AddBackslash(DestDir) + FindRec.Name;

          if (FindRec.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then begin
            if not CopyDirectoryRecursive(SourcePath, DestPath) then
              Result := False;
          end else begin
            if not FileCopy(SourcePath, DestPath, False) then
              Result := False;
          end;
        end;
      until not FindNext(FindRec);
    finally
      FindClose(FindRec);
    end;
  end;
end;

function UninstallExistingVersion(): Boolean;
var
  ResultCode: Integer;
begin
  Result := True;

  if PreviousUninstallerPath = '' then
    exit;

  if not FileExists(PreviousUninstallerPath) then begin
    MsgBox('Existing {#MyAppName} uninstaller was not found:' + #13#10 +
      PreviousUninstallerPath, mbError, MB_OK);
    Result := False;
    exit;
  end;

  Result := Exec(
    PreviousUninstallerPath,
    '/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS /FORCECLOSEAPPLICATIONS',
    '',
    SW_HIDE,
    ewWaitUntilTerminated,
    ResultCode);

  if (not Result) or (ResultCode <> 0) then begin
    MsgBox('Failed to uninstall the existing {#MyAppName} installation.' + #13#10 +
      'Exit code: ' + IntToStr(ResultCode), mbError, MB_OK);
    Result := False;
  end;
end;

function FindExistingInstallation(): Boolean;
var
  UninstallString: string;
begin
  Result := False;
  PreviousInstallDir := '';
  PreviousUninstallerPath := '';

  if QueryExistingInstallValue('UninstallString', UninstallString) then begin
    PreviousUninstallerPath := ExtractExecutablePath(UninstallString);
    Result := PreviousUninstallerPath <> '';
  end;

  QueryExistingInstallValue('InstallLocation', PreviousInstallDir);
  if (PreviousInstallDir = '') and (PreviousUninstallerPath <> '') then
    PreviousInstallDir := ExtractFileDir(PreviousUninstallerPath);

  if (not Result) and FileExists(ExpandConstant('{autopf}\{#MyAppName}\unins000.exe')) then begin
    PreviousInstallDir := ExpandConstant('{autopf}\{#MyAppName}');
    PreviousUninstallerPath := AddBackslash(PreviousInstallDir) + 'unins000.exe';
    Result := True;
  end;
end;

function InitializeSetup(): Boolean;
var
  PreviousConfigDir: string;
begin
  Result := True;
  KeepExistingConfig := False;
  PreviousInstallDir := '';
  PreviousUninstallerPath := '';
  ConfigBackupDir := ExpandConstant('{tmp}\{#MyAppName}_config_backup');

  if not FindExistingInstallation() then
    exit;

  if PreviousInstallDir <> '' then
    PreviousConfigDir := AddBackslash(PreviousInstallDir) + 'config'
  else
    PreviousConfigDir := '';

  KeepExistingConfig := False;
  if (PreviousConfigDir <> '') and DirExists(PreviousConfigDir) then begin
    KeepExistingConfig :=
      MsgBox('A previous {#MyAppName} installation was found.' + #13#10#13#10 +
        'Do you want to keep the existing config directory?',
        mbConfirmation, MB_YESNO) = IDYES;

    if KeepExistingConfig then begin
      DelTree(ConfigBackupDir, True, True, True);
      if not CopyDirectoryRecursive(PreviousConfigDir, ConfigBackupDir) then begin
        MsgBox('Failed to back up the existing config directory.', mbError, MB_OK);
        Result := False;
        exit;
      end;
    end;
  end;

  if not UninstallExistingVersion() then begin
    Result := False;
    exit;
  end;

  if (not KeepExistingConfig) and (PreviousConfigDir <> '') and DirExists(PreviousConfigDir) then
    DelTree(PreviousConfigDir, True, True, True);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and KeepExistingConfig and DirExists(ConfigBackupDir) then begin
    if not CopyDirectoryRecursive(ConfigBackupDir, ExpandConstant('{app}\config')) then
      MsgBox('Failed to restore the existing config directory.', mbError, MB_OK);

    DelTree(ConfigBackupDir, True, True, True);
  end;
end;
