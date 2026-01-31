; ==============================================================================
; Playlisted2 Inno Setup Script (64-Bit Fixed)
; ==============================================================================
#define MyAppName "Playlisted2"
#define MyAppVersion "2.0.0"
#define MyAppPublisher "Fanan"
#define MyAppURL "https://www.fanan.com"

; Define Project Root for Assets
#define ProjectRoot "D:\Workspace\onstage_with_chatgpt_10"

[Setup]
AppId={{D1A2B3C4-E5F6-4A3B-8C9D-0E1F2A3B4C5D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

; --- 64-BIT ARCHITECTURE SETTINGS (CRITICAL FIX) ---
; This ensures {commoncf} maps to "C:\Program Files\Common Files"
; instead of "C:\Program Files (x86)\Common Files"
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

; --- INSTALLATION PATH SETTINGS ---
; Default path (can be overridden by components below)
DefaultDirName={commoncf}\VST3\Playlisted2.vst3
DisableDirPage=yes
UsePreviousAppDir=no

; --- UI SETTINGS ---
DisableProgramGroupPage=yes
AllowNoIcons=yes
WizardStyle=modern

; --- VISUAL ASSETS ---
LicenseFile={#ProjectRoot}\License File EULA.txt
SetupIconFile={#ProjectRoot}\icon.ico
WizardSmallImageFile={#ProjectRoot}\WizardSmallImage.bmp

Compression=zip
SolidCompression=yes

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 Plugin (Bundle)"; Types: full custom
Name: "clap"; Description: "CLAP Plugin (Bundle)"; Types: full custom

[Dirs]
; Ensure the destination bundle directories exist
Name: "{commoncf}\VST3\Playlisted2.vst3\Contents\x86_64-win"; Components: vst3
Name: "{commoncf}\CLAP\Playlisted2.clap\Contents\x86_64-win"; Components: clap

[Files]
; ==============================================================================
; COMPONENT: VST3
; Source: The folder you confirmed contains ALL needed files
; ==============================================================================
Source: "C:\Program Files\Common Files\VST3\Playlisted2.vst3\Contents\x86_64-win\*"; DestDir: "{commoncf}\VST3\Playlisted2.vst3\Contents\x86_64-win"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

; ==============================================================================
; COMPONENT: CLAP
; Source: The folder you confirmed contains ALL needed files
; ==============================================================================
Source: "C:\Program Files\Common Files\CLAP\Playlisted2.CLAP\Contents\x86_64-win\*"; DestDir: "{commoncf}\CLAP\Playlisted2.clap\Contents\x86_64-win"; Components: clap; Flags: ignoreversion recursesubdirs createallsubdirs