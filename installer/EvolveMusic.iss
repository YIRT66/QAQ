#define MyAppName "EvolveMusic"
#ifndef MyAppVersion
  #define MyAppVersion "0.18.1"
#endif
#define MyAppPublisher "EvolveMusic Beta"
#define MyAppExeName "EvolveMusic.exe"

[Setup]
AppId={{6B415A52-6C9F-4E22-A5EA-4D3FE9910401}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\EvolveMusic
DefaultGroupName=EvolveMusic
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\dist
OutputBaseFilename=EvolveMusic-Setup-{#MyAppVersion}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=..\assets\evolvemusic.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
RestartApplications=yes
AllowNoIcons=yes
UsePreviousAppDir=yes
UsePreviousTasks=no
VersionInfoVersion={#MyAppVersion}.0
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoDescription=EvolveMusic Desktop Beta Installer

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "快捷方式："; Flags: unchecked
Name: "autostart"; Description: "登录 Windows 后自动在后台启动 EvolveMusic"; GroupDescription: "启动选项："; Flags: unchecked

[Files]
Source: "..\dist\app\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\EvolveMusic"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{autodesktop}\EvolveMusic"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "EvolveMusic"; ValueData: """{app}\{#MyAppExeName}"" --autostart --background"; Flags: uninsdeletevalue; Tasks: autostart

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 EvolveMusic"; Flags: nowait postinstall skipifsilent

[Code]
var
  DisclaimerPage: TWizardPage;
  DisclaimerMemo: TNewMemo;
  AcceptDisclaimer: TNewCheckBox;

procedure InitializeWizard;
begin
  DisclaimerPage := CreateCustomPage(wpSelectDir,
    '测试版说明与免责声明',
    '安装前请阅读并确认以下内容');

  DisclaimerMemo := TNewMemo.Create(DisclaimerPage);
  DisclaimerMemo.Parent := DisclaimerPage.Surface;
  DisclaimerMemo.Left := 0;
  DisclaimerMemo.Top := 0;
  DisclaimerMemo.Width := DisclaimerPage.SurfaceWidth;
  DisclaimerMemo.Height := ScaleY(245);
  DisclaimerMemo.ReadOnly := True;
  DisclaimerMemo.ScrollBars := ssVertical;
  DisclaimerMemo.Text :=
    'EvolveMusic 当前为测试版。' + #13#10 + #13#10 +
    '1. 本软件提供音乐服务聚合、播放器、网页播放壳、歌单与同步等功能。用户应遵守所在地法律法规以及所使用音乐平台的服务条款、版权和内容许可要求。' + #13#10 + #13#10 +
    '2. EvolveMusic 账号、收藏、历史、自建歌单与偏好可同步到项目运营者配置的 Cloudflare Worker / D1。测试阶段的云端服务可能中断、迁移、重置或停止，请勿将其作为唯一的数据备份。' + #13#10 + #13#10 +
    '3. 软件可能调用第三方网站或服务。第三方服务的可用性、内容、账号权益和隐私政策由对应服务提供方负责。' + #13#10 + #13#10 +
    '4. 自动更新会从 EvolveMusic 配置的更新地址下载安装包；如果服务端提供 SHA-256，客户端会在安装前进行校验。' + #13#10 + #13#10 +
    '5. 测试版按现状提供，可能存在缺陷。继续安装即表示你理解测试风险并同意仅将软件用于合法用途。';

  AcceptDisclaimer := TNewCheckBox.Create(DisclaimerPage);
  AcceptDisclaimer.Parent := DisclaimerPage.Surface;
  AcceptDisclaimer.Left := 0;
  AcceptDisclaimer.Top := DisclaimerMemo.Top + DisclaimerMemo.Height + ScaleY(12);
  AcceptDisclaimer.Width := DisclaimerPage.SurfaceWidth;
  AcceptDisclaimer.Caption := '我已阅读并同意上述测试版说明与免责声明';
  AcceptDisclaimer.Checked := False;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = DisclaimerPage.ID then
  begin
    if not AcceptDisclaimer.Checked then
    begin
      MsgBox('请阅读并勾选同意测试版说明与免责声明后继续。', mbInformation, MB_OK);
      Result := False;
    end;
  end;
end;
