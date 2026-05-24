#define MyAppName "qimgv-plus"
#define MyAppVersion "1.0.4.0"
#define MyAppPublisher "hadoooooouken"
#define MyAppURL "https://github.com/hadoooooouken/qimgv-plus"
#define MyAppExeName "qimgv-plus.exe"
#define MyAppIconName "qimgv.ico"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
AppId={{9F57BDCD-C82D-4EBD-93F9-8260CDA2EA53}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
VersionInfoVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DisableProgramGroupPage=yes
; PrivilegesRequired=lowest allows non-admin installation. Can be overridden in dialog.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=..\INSTALL
OutputBaseFilename=qimgv-plus-setup
SetupIconFile=res\icons\common\logo\app\{#MyAppIconName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}
CloseApplications=yes
CloseApplicationsFilter={#MyAppExeName}
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "ukrainian"; MessagesFile: "compiler:Languages\Ukrainian.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"

[CustomMessages]
; Compacting status
english.CompactingFiles=Optimizing file sizes on disk...
ukrainian.CompactingFiles=Оптимізація розміру файлів на диску...
german.CompactingFiles=Dateigrößen auf dem Datenträger optimieren...
spanish.CompactingFiles=Optimizar el tamaño de los archivos en el disco...
french.CompactingFiles=Optimisation de la taille des fichiers sur le disque...
japanese.CompactingFiles=ディスク上のファイルサイズを最適化しています...
turkish.CompactingFiles=Disk üzerindeki dosya boyutları optimize ediliyor...

; Group names
english.GroupFileAssociations=File Associations:
ukrainian.GroupFileAssociations=Асоціації файлів:
german.GroupFileAssociations=Dateizuordnungen:
spanish.GroupFileAssociations=Asociaciones de archivos:
french.GroupFileAssociations=Associations de fichiers :
japanese.GroupFileAssociations=ファイル関連付け:
turkish.GroupFileAssociations=Dosya İlişkilendirmeleri:

english.GroupAdditionalOptions=Additional options:
ukrainian.GroupAdditionalOptions=Додаткові параметри:
german.GroupAdditionalOptions=Zusätzliche Optionen:
spanish.GroupAdditionalOptions=Opciones adicionales:
french.GroupAdditionalOptions=Options supplémentaires :
japanese.GroupAdditionalOptions=追加オプション:
turkish.GroupAdditionalOptions=Ek seçenekler:

; Task description for compacting
english.CompactFilesTaskDesc=Optimize file sizes on disk (compress with LZX)
ukrainian.CompactFilesTaskDesc=Оптимізувати розмір файлів на диску (стиснення LZX)
german.CompactFilesTaskDesc=Dateigrößen auf Datenträger optimieren (LZX-Komprimierung)
spanish.CompactFilesTaskDesc=Optimizar el tamaño de los archivos en disco (compresión LZX)
french.CompactFilesTaskDesc=Optimiser la taille des fichiers sur le disque (compression LZX)
japanese.CompactFilesTaskDesc=ディスク上のファイルサイズを最適化 (LZX圧縮)
turkish.CompactFilesTaskDesc=Dosya boyutlarını diskte optimize et (LZX ile sıkıştır)

; Task description for thumbnails
english.ThumbnailsTaskDesc=Generate thumbnails in Windows Explorer
ukrainian.ThumbnailsTaskDesc=Створювати мініатюри у Провіднику Windows
german.ThumbnailsTaskDesc=Minivorschauen im Windows-Explorer generieren
spanish.ThumbnailsTaskDesc=Generar miniaturas en el Explorador de Windows
french.ThumbnailsTaskDesc=Générer des miniatures dans l'Explorateur Windows
japanese.ThumbnailsTaskDesc=Windowsエクスプローラーでサムネイルを生成
turkish.ThumbnailsTaskDesc=Windows Gezgini'nde küçük resimler oluştur

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "associate"; Description: "Associate {#MyAppName} with common image file formats"; GroupDescription: "{cm:GroupFileAssociations}"
Name: "compact"; Description: "{cm:CompactFilesTaskDesc}"; GroupDescription: "{cm:GroupAdditionalOptions}"
Name: "thumbnails"; Description: "{cm:ThumbnailsTaskDesc}"; GroupDescription: "{cm:GroupAdditionalOptions}"

[Files]
Source: "..\release\{#MyAppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\release\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "{#MyAppExeName},cache\*,conf\*,thumbnails\*"
Source: "res\icons\common\logo\app\{#MyAppIconName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "res\filetypes\*"; DestDir: "{app}\res\filetypes"; Flags: ignoreversion
Source: "..\release\qimgvshellex.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; IconFilename: "{app}\{#MyAppIconName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon; IconFilename: "{app}\{#MyAppIconName}"

[Run]
Filename: "compact.exe"; Parameters: "/c /f /s:""{app}"" /exe:lzx /i"; WorkingDir: "{app}"; StatusMsg: "{cm:CompactingFiles}"; Flags: runhidden; Tasks: compact
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent runasoriginaluser
Filename: "regsvr32.exe"; Parameters: "/s ""{app}\qimgvshellex.dll"""; WorkingDir: "{app}"; Flags: runhidden; Tasks: thumbnails

[UninstallRun]
Filename: "regsvr32.exe"; Parameters: "/u /s ""{app}\qimgvshellex.dll"""; WorkingDir: "{app}"; Flags: runhidden; RunOnceId: "UninstallShellex"

[Registry]
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "qimgv-plus"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

; SupportedTypes for Applications
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".jpg"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".jpeg"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".jpe"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".jfif"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".png"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".gif"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".webp"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".bmp"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".dib"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".tiff"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".tif"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".tga"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".svg"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".svgz"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ico"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".avif"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".heic"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".heif"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".jxl"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".psd"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".dng"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".cr2"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".cr3"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".nef"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".arw"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".orf"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".rw2"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".pef"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".raf"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".raw"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".kra"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ora"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".exr"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".hdr"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".jxr"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".hdp"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".wdp"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".ai"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".pdf"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\Applications\{#MyAppExeName}\SupportedTypes"; ValueType: string; ValueName: ".psb"; ValueData: ""; Flags: uninsdeletekey; Tasks: associate

; Default value and OpenWithProgids for Extensions
Root: HKA; Subkey: "Software\Classes\.jpg"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.jpg"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jpg\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.jpg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jpeg"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.jpeg"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jpeg\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.jpeg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jpe"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.jpg"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jpe\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.jpg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jfif"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.jpg"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jfif\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.jpg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.png"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.png"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.png\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.png"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.gif"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.gif"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.gif\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.gif"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.webp"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.webp"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.webp\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.webp"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.bmp"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.bmp"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.bmp\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.bmp"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.dib"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.bmp"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.dib\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.bmp"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.tiff"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.tiff"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.tiff\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.tiff"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.tif"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.tif"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.tif\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.tif"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.tga"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.tga"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.tga\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.tga"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.svg"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.svg"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.svg\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.svg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.svgz"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.svg"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.svgz\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.svg"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.ico"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.ico"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.ico\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.ico"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.avif"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.avif"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.avif\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.avif"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.heic"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.heic"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.heic\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.heic"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.heif"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.heif"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.heif\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.heif"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jxl"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.jxl"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jxl\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.jxl"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.psd"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.psd"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.psd\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.psd"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.dng"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.dng\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.cr2"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.cr2\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.cr3"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.cr3\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.nef"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.nef\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.arw"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.arw\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.orf"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.orf\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.rw2"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.rw2\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.pef"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.pef\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.raf"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.raf\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.raw"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.raw\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.raw"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.kra"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.kra"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.kra\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.kra"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.ora"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.ora"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.ora\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.ora"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.exr"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.exr"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.exr\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.exr"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.hdr"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.hdr"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.hdr\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.hdr"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jxr"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.jxr"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.jxr\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.jxr"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.hdp"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.jxr"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.hdp\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.jxr"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.wdp"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.jxr"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.wdp\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.jxr"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.ai"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.ai"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.ai\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.ai"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.pdf"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.pdf"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.pdf\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.pdf"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.psb"; ValueType: string; ValueName: ""; ValueData: "qimgvplus.AssocFile.psb"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\Classes\.psb\OpenWithProgids"; ValueType: string; ValueName: "qimgvplus.AssocFile.psb"; ValueData: ""; Flags: uninsdeletevalue; Tasks: associate

; Associate ProgID details
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile"; ValueType: string; ValueName: ""; ValueData: "Image File (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppIconName}"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

; Specific ProgIDs for each file type with its own icon
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.ai"; ValueType: string; ValueName: ""; ValueData: "Adobe Illustrator Artwork (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.ai\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\ai.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.ai\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.bmp"; ValueType: string; ValueName: ""; ValueData: "BMP Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.bmp\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\bmp.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.bmp\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.exr"; ValueType: string; ValueName: ""; ValueData: "OpenEXR Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.exr\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\exr.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.exr\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.gif"; ValueType: string; ValueName: ""; ValueData: "GIF Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.gif\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\gif.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.gif\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.hdr"; ValueType: string; ValueName: ""; ValueData: "Radiance HDR Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.hdr\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\hdr.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.hdr\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.heic"; ValueType: string; ValueName: ""; ValueData: "HEIC Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.heic\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\heic.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.heic\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.heif"; ValueType: string; ValueName: ""; ValueData: "HEIF Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.heif\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\heif.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.heif\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jpeg"; ValueType: string; ValueName: ""; ValueData: "JPEG Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jpeg\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\jpeg.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jpeg\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jpg"; ValueType: string; ValueName: ""; ValueData: "JPEG Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jpg\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\jpg.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jpg\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jxr"; ValueType: string; ValueName: ""; ValueData: "JPEG XR Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jxr\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\jxr.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jxr\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.pdf"; ValueType: string; ValueName: ""; ValueData: "PDF Document (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.pdf\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\pdf.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.pdf\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.png"; ValueType: string; ValueName: ""; ValueData: "PNG Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.png\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\png.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.png\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.psb"; ValueType: string; ValueName: ""; ValueData: "Photoshop Large Document (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.psb\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\psb.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.psb\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.psd"; ValueType: string; ValueName: ""; ValueData: "Photoshop Document (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.psd\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\psd.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.psd\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.raw"; ValueType: string; ValueName: ""; ValueData: "RAW Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.raw\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\raw.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.raw\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate


Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.svg"; ValueType: string; ValueName: ""; ValueData: "SVG Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.svg\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\svg.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.svg\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.tif"; ValueType: string; ValueName: ""; ValueData: "TIFF Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.tif\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\tif.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.tif\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.tiff"; ValueType: string; ValueName: ""; ValueData: "TIFF Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.tiff\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\tiff.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.tiff\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.webp"; ValueType: string; ValueName: ""; ValueData: "WebP Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.webp\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\webp.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.webp\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.tga"; ValueType: string; ValueName: ""; ValueData: "TGA Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.tga\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\tga.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.tga\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.kra"; ValueType: string; ValueName: ""; ValueData: "Krita Document (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.kra\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\kra.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.kra\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.ora"; ValueType: string; ValueName: ""; ValueData: "OpenRaster Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.ora\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\ora.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.ora\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.avif"; ValueType: string; ValueName: ""; ValueData: "AVIF Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.avif\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\avif.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.avif\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jxl"; ValueType: string; ValueName: ""; ValueData: "JPEG XL Image (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jxl\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\jxl.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.jxl\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate

Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.ico"; ValueType: string; ValueName: ""; ValueData: "Icon File (qimgv-plus)"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.ico\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\res\filetypes\ico.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\Classes\qimgvplus.AssocFile.ico\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Flags: uninsdeletekey; Tasks: associate


; Register in Default Programs (Windows Vista / 7 / 8 / 10 / 11)
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "{#MyAppName}"; ValueData: "Software\{#MyAppName}\Capabilities"; Flags: uninsdeletevalue; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "{#MyAppName}"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Fast, easy to use, and beautiful image viewer."; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpg"; ValueData: "qimgvplus.AssocFile.jpg"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpeg"; ValueData: "qimgvplus.AssocFile.jpeg"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpe"; ValueData: "qimgvplus.AssocFile.jpg"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jfif"; ValueData: "qimgvplus.AssocFile.jpg"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".png"; ValueData: "qimgvplus.AssocFile.png"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".gif"; ValueData: "qimgvplus.AssocFile.gif"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".webp"; ValueData: "qimgvplus.AssocFile.webp"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".bmp"; ValueData: "qimgvplus.AssocFile.bmp"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dib"; ValueData: "qimgvplus.AssocFile.bmp"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tiff"; ValueData: "qimgvplus.AssocFile.tiff"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tif"; ValueData: "qimgvplus.AssocFile.tif"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tga"; ValueData: "qimgvplus.AssocFile.tga"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".svg"; ValueData: "qimgvplus.AssocFile.svg"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".svgz"; ValueData: "qimgvplus.AssocFile.svg"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ico"; ValueData: "qimgvplus.AssocFile.ico"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".avif"; ValueData: "qimgvplus.AssocFile.avif"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heic"; ValueData: "qimgvplus.AssocFile.heic"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".heif"; ValueData: "qimgvplus.AssocFile.heif"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jxl"; ValueData: "qimgvplus.AssocFile.jxl"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".psd"; ValueData: "qimgvplus.AssocFile.psd"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dng"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".cr2"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".cr3"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".nef"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".arw"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".orf"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rw2"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".pef"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".raf"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".raw"; ValueData: "qimgvplus.AssocFile.raw"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".kra"; ValueData: "qimgvplus.AssocFile.kra"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ora"; ValueData: "qimgvplus.AssocFile.ora"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".exr"; ValueData: "qimgvplus.AssocFile.exr"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".hdr"; ValueData: "qimgvplus.AssocFile.hdr"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jxr"; ValueData: "qimgvplus.AssocFile.jxr"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".hdp"; ValueData: "qimgvplus.AssocFile.jxr"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".wdp"; ValueData: "qimgvplus.AssocFile.jxr"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ai"; ValueData: "qimgvplus.AssocFile.ai"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".pdf"; ValueData: "qimgvplus.AssocFile.pdf"; Flags: uninsdeletekey; Tasks: associate
Root: HKA; Subkey: "Software\{#MyAppName}\Capabilities\FileAssociations"; ValueType: string; ValueName: ".psb"; ValueData: "qimgvplus.AssocFile.psb"; Flags: uninsdeletekey; Tasks: associate