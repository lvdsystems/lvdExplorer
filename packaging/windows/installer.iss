; Inno Setup script for lvdExplorer. Compiled via build-installer.ps1,
; which stages a Release build (see stage-release.ps1) and passes its
; path in via the StagingDir preprocessor define -- run ISCC directly
; against this file only if StagingDir already exists at the fallback
; path below.
#ifndef StagingDir
  #define StagingDir "..\..\build\release-package\staging"
#endif
#ifndef OutputDir
  #define OutputDir "..\..\dist"
#endif
; build-installer.ps1 passes the actual project version in via
; /DMyAppVersion=..., read straight out of the Release build it just
; produced (see stage-release.ps1's Version.h parsing) -- this literal is
; only the fallback for a direct, manual ISCC invocation.
#ifndef MyAppVersion
  #define MyAppVersion "0.1.0"
#endif

#define MyAppName "lvdExplorer"
#define MyAppPublisher "lvd Systems"
#define MyAppExeName "lvdExplorer.exe"

[Setup]
; Stable per-app identifier so repeat installs/upgrades are recognized as
; the same product rather than a fresh install each time -- generated
; once for this project, never reuse it for a different app.
AppId={{04910579-AD14-42C2-8AE2-BFBAEA57561C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
OutputDir={#OutputDir}
OutputBaseFilename=lvdExplorer-{#MyAppVersion}-setup-win64
; The installer .exe's own file-properties version (Explorer -> Properties
; -> Details), separate from AppVersion above (which is the *product*
; version Inno Setup itself tracks for upgrade detection).
VersionInfoVersion={#MyAppVersion}.0
VersionInfoTextVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=lvdExplorer Setup
Compression=lzma2
SolidCompression=yes
LicenseFile=..\..\LICENSE
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent
