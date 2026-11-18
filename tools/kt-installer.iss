[Setup]
AppId={{5F17963F-3CBD-4E14-9DBA-522232B0BB7A}
AppName=KT
AppVersion={#GameVersion}
VersionInfoVersion={#GameVersion}.0
AppPublisher=KT
AppPublisherURL=https://github.com/creativekamrul/kt
AppSupportURL=https://github.com/creativekamrul/kt/issues
DefaultDirName={localappdata}\Programs\KT
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
UninstallDisplayName=KT
UninstallDisplayIcon={app}\KT.exe
OutputDir=..\out
OutputBaseFilename=KT-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Tasks]
Name: "desktopicon"; Description: "Create a Play KT shortcut on my desktop"; Flags: unchecked

[Files]
Source: "{#GamePath}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{userprograms}\KT"; Filename: "{app}\KT.exe"; WorkingDir: "{app}"
Name: "{userdesktop}\Play KT"; Filename: "{app}\KT.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\KT.exe"; WorkingDir: "{app}"; Description: "Play KT"; Flags: postinstall nowait skipifsilent
