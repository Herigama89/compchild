; Black Tube — Windows installer (VST3, 64-bit). Built by CI with Inno Setup 6.
#ifndef AppVersion
  #define AppVersion "1.1.0"
#endif

[Setup]
AppId={{6B3C1E2A-8F4D-4C1B-9A7E-B1AC7B0BE001}
AppName=Black Tube
AppVersion={#AppVersion}
AppPublisher=Herigama Audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousAppDir=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=BlackTube-{#AppVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
UninstallDisplayName=Black Tube (VST3)
WizardStyle=modern

[Files]
Source: "..\..\build\BlackTube\BlackTube_artefacts\Release\VST3\Black Tube.vst3\*"; DestDir: "{commoncf64}\VST3\Black Tube.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\Black Tube.vst3"
