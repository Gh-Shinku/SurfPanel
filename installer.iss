; -- installer.iss --

#define MyAppName "SurfPanel"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Shinku"
#define MyAppURL "https://github.com/Gh-Shinku/SurfPanel"
#define MyAppExeName "SurfPanel.exe"
#define MyAppId "{{4D2F8EC0-8A04-43EF-B8C8-D8F847A16D96}}"
#define MyProjectRoot SourcePath
#define MyMingwRoot GetEnv("SURFPANEL_MINGW_ROOT")
#if MyMingwRoot == ""
  #error "SURFPANEL_MINGW_ROOT is required and must point to the MinGW installation prefix"
#endif
#define MyMingwBin AddBackslash(MyMingwRoot) + "bin"
#define MyBuildDir AddBackslash(MyProjectRoot) + "build"
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
CloseApplications=yes
CloseApplicationsFilter={#MyAppExeName}
RestartApplications=no
UsePreviousAppDir=yes
UsePreviousTasks=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "Start SurfPanel with Windows"; Flags: unchecked

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

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
function PrepareToInstall(var NeedsRestart: Boolean): String;
var
  ResultCode: Integer;
begin
  { SurfPanel is a background daemon with no unsaved documents. Stop every
    running instance before Restart Manager scans files so upgrades proceed
    without an application-closing confirmation page. }
  Exec(ExpandConstant('{sys}\taskkill.exe'),
    '/F /IM "{#MyAppExeName}"', '', SW_HIDE, ewWaitUntilTerminated,
    ResultCode);
  Result := '';
end;
