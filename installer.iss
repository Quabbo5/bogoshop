; Bogoshop Inno Setup Installer Script
; Kompilieren: Inno Setup öffnen → diese Datei laden → Compile (Strg+F9)
; Output: installer/Bogoshop_Setup_0.2.1_PRE_RELEASE.exe

#define AppName      "Bogoshop"
#define AppVersion   "0.2.1 PRE-RELEASE"
#define AppExe       "viewer.exe"
#define AppPublisher "Bogoshop"

[Setup]
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppId={{B0G05H0P-0002-0001-PRE0-RELEASE000001}

; Installationsordner: C:\Program Files\Bogoshop
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes

; Output
OutputDir=installer
OutputBaseFilename=Bogoshop_Setup_0.2.1_PRE_RELEASE
Compression=lzma2/ultra64
SolidCompression=yes

; Icon (aus viewer.exe extrahieren)
SetupIconFile=assets\icon.ico

; Windows 10+ empfohlen
MinVersion=10.0

; Kein Admin nötig (installiert in Program Files trotzdem mit UAC-Prompt)
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "german";  MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
; Desktop-Shortcut (Checkbox im Installer)
Name: "desktopicon"; Description: "Add Bogoshop to Desktop"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
; Hauptprogramm
Source: "viewer.exe";                    DestDir: "{app}"; Flags: ignoreversion
; SDL2-Laufzeitbibliotheken
Source: "C:\msys64\mingw64\bin\SDL2.dll";             DestDir: "{app}"; Flags: ignoreversion
Source: "C:\msys64\mingw64\bin\libwinpthread-1.dll";  DestDir: "{app}"; Flags: ignoreversion
; Dokumentation
Source: "docs\README.txt";              DestDir: "{app}\docs"; Flags: ignoreversion
Source: "docs\effects\*";              DestDir: "{app}\docs\effects"; Flags: ignoreversion

[Icons]
; Start-Menü
Name: "{group}\{#AppName}";          Filename: "{app}\{#AppExe}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
; Desktop (nur wenn Task ausgewählt)
Name: "{autodesktop}\{#AppName}";    Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; "Open after install"-Checkbox
Filename: "{app}\{#AppExe}"; Description: "Open Bogoshop after install"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; AppData beim Deinstallieren NICHT löschen (Lizenz bleibt erhalten)
; Falls du die Lizenz beim Deinstallieren entfernen willst, auskommentieren:
; Type: filesandordirs; Name: "{userappdata}\bogoshop"
