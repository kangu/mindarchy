#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef StageDir
  #error StageDir must contain the deployed application
#endif
#ifndef OutputPath
  #define OutputPath "output"
#endif

[Setup]
AppId={{8BF45D6C-53F3-4AF0-B3D2-64A39CB58F1D}
AppName=Mindarchy
AppVersion={#AppVersion}
AppPublisher=Mindarchy
DefaultDirName={localappdata}\Programs\Mindarchy
DefaultGroupName=Mindarchy
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
OutputDir={#OutputPath}
OutputBaseFilename=Mindarchy-{#AppVersion}-windows-x64-setup
SetupIconFile=..\..\assets\icons\mindarchy.ico
UninstallDisplayIcon={app}\mindarchy.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ChangesAssociations=yes
CloseApplications=yes
RestartApplications=no

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Mindarchy"; Filename: "{app}\mindarchy.exe"
Name: "{autodesktop}\Mindarchy"; Filename: "{app}\mindarchy.exe"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Classes\.omm\OpenWithProgids"; ValueType: string; ValueName: "Mindarchy.Document"; ValueData: ""; Flags: uninsdeletevalue
Root: HKCU; Subkey: "Software\Classes\.omm"; ValueType: string; ValueName: ""; ValueData: "Mindarchy.Document"; Check: NoOmmAssociation
Root: HKCU; Subkey: "Software\Classes\Mindarchy.Document"; ValueType: string; ValueName: ""; ValueData: "Mindarchy mind map"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Mindarchy.Document\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\mindarchy.exe"",0"
Root: HKCU; Subkey: "Software\Classes\Mindarchy.Document\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\mindarchy.exe"" --document ""%1"""
Root: HKCU; Subkey: "Software\Classes\Applications\mindarchy.exe"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "Mindarchy"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\Classes\Applications\mindarchy.exe\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\mindarchy.exe"" --document ""%1"""
Root: HKCU; Subkey: "Software\Classes\Applications\mindarchy.exe\SupportedTypes"; ValueType: string; ValueName: ".omm"; ValueData: ""

[Run]
Filename: "{app}\mindarchy.exe"; Description: "Launch Mindarchy"; Flags: nowait postinstall skipifsilent

[Code]
function NoOmmAssociation: Boolean;
var Existing: String;
begin
  Result := not RegQueryStringValue(HKCU, 'Software\Classes\.omm', '', Existing) or (Existing = '') or (Existing = 'Mindarchy.Document');
end;

var
  PreserveOmmAssociation: Boolean;
  PreviousOmmAssociation: String;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var Existing: String;
begin
  if CurUninstallStep = usUninstall then
    PreserveOmmAssociation :=
      RegQueryStringValue(HKCU, 'Software\Classes\.omm', '', PreviousOmmAssociation) and
      (PreviousOmmAssociation <> 'Mindarchy.Document');
  if CurUninstallStep = usPostUninstall then begin
    { Upgrade uninstall logs may contain older unconditional cleanup entries. }
    if PreserveOmmAssociation then
      RegWriteStringValue(HKCU, 'Software\Classes\.omm', '', PreviousOmmAssociation)
    else if RegQueryStringValue(HKCU, 'Software\Classes\.omm', '', Existing) and
            (Existing = 'Mindarchy.Document') then
      RegDeleteValue(HKCU, 'Software\Classes\.omm', '');
  end;
end;
