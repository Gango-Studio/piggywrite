; piggywrite installer
; Build with Inno Setup 7: ISCC.exe installer\piggywrite.iss
; Installs per-user (no administrator rights), adds {app} to the user PATH,
; and creates Start Menu / optional desktop shortcuts.

#define MyAppName "piggywrite"
#define MyAppVersion "1.0"
#define MyAppPublisher "Gango Studios & Sam Mu"
#define MyAppExeName "piggywrite.exe"

[Setup]
AppId={{0FA6442C-12DC-483B-B2F1-15277406576F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://gango-studio.github.io/
AppSupportURL=https://gango-studio.github.io/
AppUpdatesURL=https://gango-studio.github.io/
AppCopyright=Copyright (C) 2026 Gango Studios & Sam Mu
DefaultDirName={localappdata}\Programs\piggywrite
DefaultGroupName=piggywrite
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir=dist
OutputBaseFilename=piggywrite-{#MyAppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ChangesEnvironment=yes
UninstallDisplayIcon={app}\{#MyAppExeName}
VersionInfoVersion=1.0.0.0
VersionInfoDescription=piggywrite 安装程序
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyright=Copyright (C) 2026 Gango Studios & Sam Mu
VersionInfoProductName=piggywrite
VersionInfoProductVersion=1.0.0.0

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\build\piggywrite.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\piggywrite"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{userdocs}"; Comment: "piggywrite 命令行文本编辑器"
Name: "{autodesktop}\piggywrite"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{userdocs}"; Tasks: desktopicon; Comment: "piggywrite 命令行文本编辑器"

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 piggywrite"; WorkingDir: "{userdocs}"; Flags: nowait postinstall skipifsilent unchecked

[Messages]
FinishedHeadingLabel=完成 piggywrite 安装
FinishedLabel=piggywrite 已安装完成。%n%n请重新打开 PowerShell 或 Windows Terminal，然后输入：%n%n    piggywrite 文件名
FinishedLabelNoIcons=piggywrite 已安装完成。%n%n请重新打开 PowerShell 或 Windows Terminal，然后输入：%n%n    piggywrite 文件名
ClickFinish=点击“完成”退出安装程序。

[Code]
const
  UserEnvironmentKey = 'Environment';

function NormalizePathEntry(const Entry: string): string;
begin
  Result := Trim(Entry);
  while (Length(Result) > 0) and (Result[Length(Result)] = '\') do
    Delete(Result, Length(Result), 1);
end;

function PathContainsEntry(const PathList, Entry: string): Boolean;
var
  Rest, Item: string;
  Semicolon: Integer;
begin
  Result := False;
  Rest := PathList;
  while Rest <> '' do
  begin
    Semicolon := Pos(';', Rest);
    if Semicolon = 0 then
    begin
      Item := Rest;
      Rest := '';
    end
    else
    begin
      Item := Copy(Rest, 1, Semicolon - 1);
      Delete(Rest, 1, Semicolon);
    end;

    if (Item <> '') and
       (CompareText(NormalizePathEntry(Item), NormalizePathEntry(Entry)) = 0) then
    begin
      Result := True;
      Exit;
    end;
  end;
end;

function GetUserPath: string;
begin
  if not RegQueryStringValue(HKCU, UserEnvironmentKey, 'Path', Result) then
    Result := '';
end;

procedure AddAppToUserPath;
var
  CurrentPath, NewPath, AppDir: string;
begin
  AppDir := ExpandConstant('{app}');
  CurrentPath := GetUserPath;

  if PathContainsEntry(CurrentPath, AppDir) then
    Exit;

  NewPath := AppDir;
  if CurrentPath <> '' then
    NewPath := NewPath + ';' + CurrentPath;

  if not RegWriteExpandStringValue(HKCU, UserEnvironmentKey, 'Path', NewPath) then
    MsgBox('无法自动把 piggywrite 添加到 PATH。' + #13#10 +
           '请手动把下面的目录加入用户 PATH：' + #13#10 + AppDir,
           mbError, MB_OK);
end;

function RemovePathEntry(const PathList, Entry: string): string;
var
  Rest, Item: string;
  Semicolon: Integer;
begin
  Result := '';
  Rest := PathList;
  while Rest <> '' do
  begin
    Semicolon := Pos(';', Rest);
    if Semicolon = 0 then
    begin
      Item := Rest;
      Rest := '';
    end
    else
    begin
      Item := Copy(Rest, 1, Semicolon - 1);
      Delete(Rest, 1, Semicolon);
    end;

    if (Item <> '') and
       (CompareText(NormalizePathEntry(Item), NormalizePathEntry(Entry)) <> 0) then
    begin
      if Result <> '' then
        Result := Result + ';';
      Result := Result + Item;
    end;
  end;
end;

procedure RemoveAppFromUserPath;
var
  CurrentPath, NewPath, AppDir: string;
begin
  AppDir := ExpandConstant('{app}');
  CurrentPath := GetUserPath;
  NewPath := RemovePathEntry(CurrentPath, AppDir);

  if NewPath <> CurrentPath then
    RegWriteExpandStringValue(HKCU, UserEnvironmentKey, 'Path', NewPath);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    AddAppToUserPath;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
    RemoveAppFromUserPath;
end;
