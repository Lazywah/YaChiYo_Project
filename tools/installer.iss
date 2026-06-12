; YaChiYo Desktop Pet - Inno Setup installer script
; -------------------------------------------------------------------
; Build steps:
;   1. Build a Release exe (Qt Creator / CMake)
;   2. Run  tools\deploy.ps1   to produce  dist\YaChiYo\
;   3. Compile this script with Inno Setup (ISCC.exe installer.iss),
;      or open it in the Inno Setup IDE and press Build.
;
; Requires: Inno Setup 6  (https://jrsoftware.org/isdl.php)
; Output:   tools\Output\YaChiYo-Setup-<version>.exe
; -------------------------------------------------------------------

#define AppName        "YaChiYo Desktop Pet"
#define AppVersion     "0.1.0"
#define AppPublisher   "YaChiYo"
#define AppExeName     "YaChiYo_Project.exe"

; Paths are relative to this .iss file (tools\)
#define DistDir   "..\dist\YaChiYo"
#define IconFile  "..\resources\icons\app.ico"

[Setup]
; A fixed AppId keeps upgrades/uninstalls consistent across versions.
AppId={{8F3C2A10-9B6E-4A7D-9C21-A1B2C3D4E5F6}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=YaChiYo-Setup-{#AppVersion}
SetupIconFile={#IconFile}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; Per-user install (no admin needed); switch to "admin" for all-users.
PrivilegesRequiredOverridesAllowed=dialog commandline

[Languages]
Name: "english";  MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
; Deploy the entire self-contained folder produced by deploy.ps1
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#AppName}";        Filename: "{app}\{#AppExeName}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";  Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[Registry]
; The app may add an auto-start entry under HKCU\...\Run while running.
; Do not create it here; only remove it on uninstall so nothing lingers.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "YaChiYo"; Flags: dontcreatekey uninsdeletevalue
