; ChainHost.iss — Inno Setup script for Windows x64 installer
; Version is passed in via iscc /DMyAppVersion=x.y.z-suffix

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif

#define MyAppName      "ChainHost"
#define MyAppPublisher "Avious"
#define MyAppExeName   "ChainHost.exe"

[Setup]
AppId={{7C2E1C9E-4B8C-4E90-9D5B-CHAINHOST0001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppPublisher}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputDir=..\build
OutputBaseFilename=ChainHost-{#MyAppVersion}-Windows-x64
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayIcon={app}\{#MyAppExeName}

[Files]
; Standalone host
Source: "..\build\ChainHost_artefacts\Release\Standalone\{#MyAppExeName}"; \
  DestDir: "{app}"; Flags: ignoreversion

; Instrument VST3
Source: "..\build\ChainHost_artefacts\Release\VST3\ChainHost.vst3\*"; \
  DestDir: "{commoncf64}\VST3\ChainHost.vst3"; \
  Flags: recursesubdirs createallsubdirs ignoreversion

; Effect VST3
Source: "..\build\ChainHostFX_artefacts\Release\VST3\ChainHost FX.vst3\*"; \
  DestDir: "{commoncf64}\VST3\ChainHost FX.vst3"; \
  Flags: recursesubdirs createallsubdirs ignoreversion

[Icons]
Name: "{group}\{#MyAppName}";         Filename: "{app}\{#MyAppExeName}"
Name: "{group}\Uninstall {#MyAppName}"; Filename: "{uninstallexe}"
