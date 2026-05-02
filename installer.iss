; -- installer.iss --

#define MyAppName "SurfPanel"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Shinku"
#define MyAppURL "https://github.com/shinku/SurfPanel"
#define MyAppExeName "SurfPanel.exe"
#define MyAppId "{{4D2F8EC0-8A04-43EF-B8C8-D8F847A16D96}}"
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
PrivilegesRequired=admin
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

[Dirs]
Name: "{app}\config"

[Files]
; Main executable
Source: "{#MyBuildDir}\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion

; Qt and runtime DLLs deployed beside the executable
Source: "{#MyBuildDir}\*.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#MyBuildDir}\D3Dcompiler_47.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyBuildDir}\dxcompiler.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyBuildDir}\dxil.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

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
Source: "{#MyMingwBin}\libicuin78.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libicuuc78.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libdouble-conversion.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libpcre2-16-0.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libzstd.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libbz2-1.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libbrotlidec.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libglib-2.0-0.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libgraphite2.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libbrotlicommon.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libintl-8.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libicudt78.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libpcre2-8-0.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#MyMingwBin}\libiconv-2.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

; Qt plugin directories produced by windeployqt
Source: "{#MyBuildDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MyBuildDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#MyBuildDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#MyBuildDir}\generic\*"; DestDir: "{app}\generic"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#MyBuildDir}\networkinformation\*"; DestDir: "{app}\networkinformation"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#MyBuildDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "{#MyBuildDir}\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

; Ship initial config files with the app
Source: "{#MyConfigDir}\*"; DestDir: "{app}\config"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; IconFilename: "{app}\{#MyAppExeName}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent