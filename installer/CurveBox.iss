#ifndef PluginFormat
  #error PluginFormat must be VST3 or VST2
#endif
#ifndef PayloadDir
  #error PayloadDir must point to the built plugin
#endif
#if PluginFormat == "VST3"
  #define PluginId "CurveBox-VST3"
  #define Destination "{commoncf64}\VST3"
#elif PluginFormat == "VST2"
  #define PluginId "CurveBox-VST2"
  #define Destination "{autopf64}\VstPlugins"
#else
  #error Unsupported plugin format
#endif

[Setup]
AppId={{C6BA9914-B4DC-435C-A2EB-48EF625794AB}-{#PluginId}
AppName=CurveBox {#PluginFormat}
AppVersion=1.0.0
AppPublisher=CurveBox Audio
DefaultDirName={#Destination}
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\packages
OutputBaseFilename=CurveBox-{#PluginFormat}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=CurveBox {#PluginFormat}
UninstallFilesDir={commonappdata}\CurveBox\Uninstall\{#PluginFormat}

[Files]
#if PluginFormat == "VST3"
Source: "{#PayloadDir}\CurveBox.vst3\*"; DestDir: "{app}\CurveBox.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
#else
Source: "{#PayloadDir}\CurveBox.dll"; DestDir: "{app}"; Flags: ignoreversion
#endif
