[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$root=$PSScriptRoot
$source=Join-Path $root 'bridge-src'
$module=Join-Path $root 'bridge-module'
$tools=if($env:NS09_TOOL_ROOT){$env:NS09_TOOL_ROOT}else{[IO.Path]::GetFullPath((Join-Path $root '../.ns09-tools'))}
$build=Join-Path $root 'build';$dist=Join-Path $root 'dist'
New-Item -ItemType Directory -Path $tools,$build,$dist,(Join-Path $module 'bin') -Force | Out-Null
$zig=Join-Path $tools 'zig-x86_64-windows-0.15.2/zig.exe'
if(-not(Test-Path -LiteralPath $zig)){
 $entry=(Invoke-RestMethod -Uri 'https://ziglang.org/download/index.json').'0.15.2'.'x86_64-windows'
 $archive=Join-Path $tools 'zig-0.15.2.zip';Invoke-WebRequest -Uri $entry.tarball -OutFile $archive
 if((Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()-ne $entry.shasum){throw 'Zig archive checksum mismatch'}
 Expand-Archive -LiteralPath $archive -DestinationPath $tools -Force
}
$jdkRoot=Join-Path $tools 'jdk17'
$javac=Get-ChildItem -LiteralPath $jdkRoot -Filter javac.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
if(-not $javac){
 $entry=(Invoke-RestMethod -Uri 'https://api.adoptium.net/v3/assets/latest/17/hotspot?architecture=x64&image_type=jdk&os=windows&vendor=eclipse')[0]
 $archive=Join-Path $tools 'jdk17.zip';Invoke-WebRequest -Uri $entry.binary.package.link -OutFile $archive
 if((Get-FileHash -LiteralPath $archive).Hash.ToLowerInvariant()-ne $entry.binary.package.checksum){throw 'JDK checksum mismatch'}
 Expand-Archive -LiteralPath $archive -DestinationPath $jdkRoot -Force
 $javac=Get-ChildItem -LiteralPath $jdkRoot -Filter javac.exe -Recurse | Select-Object -First 1
}
$r8=Join-Path $tools 'r8-8.3.37.jar'
if(-not(Test-Path -LiteralPath $r8)){Invoke-WebRequest -Uri 'https://dl.google.com/dl/android/maven2/com/android/tools/r8/8.3.37/r8-8.3.37.jar' -OutFile $r8}
if((Get-FileHash -LiteralPath $r8).Hash-ne '59753E70A74F918389CC87F1B7D66B5C0862932559167425708DED159E3DE439'){throw 'R8 checksum mismatch'}
$bb=Join-Path $tools 'busybox64.exe'
if(-not(Test-Path -LiteralPath $bb)){Invoke-WebRequest -Uri 'https://frippery.org/files/busybox/busybox64.exe' -OutFile $bb}
if((Get-FileHash -LiteralPath $bb).Hash-ne '07BB1E5B095B00D68A695481F9240879F33C5724B40AA2308F999D54ED78F075'){throw 'BusyBox checksum mismatch; use the documented pinned tool artifact'}
$utf8=[Text.UTF8Encoding]::new($false)
foreach($file in Get-ChildItem -LiteralPath $module -Filter '*.sh'){
 [IO.File]::WriteAllText($file.FullName,[IO.File]::ReadAllText($file.FullName).Replace("`r`n","`n"),$utf8)
 & $bb sh -n $file.FullName;if($LASTEXITCODE){throw ('Shell syntax failed: '+$file.Name)}
}
& (Join-Path $root 'test-backend-regression.ps1')
$tests=@()
foreach($name in 'core','protocol','identity','native_contract','hid_contract','xbox_profile','target_identity'){
 $executable=Join-Path $build "test-$name.exe"
 & $zig cc -std=c11 -O2 -Wall -Wextra -Werror (Join-Path $source "test_$name.c") -o $executable
 if($LASTEXITCODE){throw ('Test compile failed: '+$name)}
 $result=& $executable;if($LASTEXITCODE){throw ('Test failed: '+$name)};$tests+=@($result)
}
$oldToolRoot=$env:NS09_TOOL_ROOT;$env:NS09_TOOL_ROOT=$tools
try{
 & (Join-Path $root 'test-bridge-startup.ps1')
 & (Join-Path $root 'test-xbox-keylayout.ps1')
}finally{$env:NS09_TOOL_ROOT=$oldToolRoot}
& $zig cc -target aarch64-linux-musl -static -std=c11 -O2 -Wall -Wextra -Werror '-Wl,-z,max-page-size=16384' '-Wl,-s' ("-ffile-prefix-map="+$root+'=.') ("-ffile-prefix-map="+$tools+'=toolchain') (Join-Path $source 'ns09_bridge.c') -o (Join-Path $module 'bin/ns09-bridged')
if($LASTEXITCODE){throw 'ARM64 daemon build failed'}
$classes=Join-Path $build 'classes';$dex=Join-Path $build 'dex'
New-Item -ItemType Directory -Path $classes,$dex -Force | Out-Null
& $javac.FullName --release 8 -d $classes (Join-Path $source 'NS09FrameworkProbe.java')
if($LASTEXITCODE){throw 'Framework probe compilation failed'}
& (Join-Path $javac.DirectoryName 'java.exe') -cp $r8 com.android.tools.r8.D8 --min-api 31 --output $dex (Join-Path $classes 'NS09FrameworkProbe.class')
if($LASTEXITCODE){throw 'DEX generation failed'}
Copy-Item -LiteralPath (Join-Path $dex 'classes.dex') -Destination (Join-Path $module 'bin/framework-probe.dex') -Force
$elf=[IO.File]::ReadAllBytes((Join-Path $module 'bin/ns09-bridged'))
if($elf[0]-ne 127-or [Text.Encoding]::ASCII.GetString($elf,1,3)-ne 'ELF'-or $elf[4]-ne 2-or [BitConverter]::ToUInt16($elf,18)-ne 183){throw 'Expected ARM64 ELF64'}
$offset=[int][BitConverter]::ToUInt64($elf,32);$size=[BitConverter]::ToUInt16($elf,54);$count=[BitConverter]::ToUInt16($elf,56)
for($i=0;$i-lt $count;$i++){
 $p=$offset+$i*$size;$type=[BitConverter]::ToUInt32($elf,$p)
 if($type-eq 3){throw 'Expected static ELF'}
 if($type-eq 1-and [BitConverter]::ToUInt64($elf,$p+48)-lt 16384){throw 'ELF alignment below16KiB'}
}
$zip=Join-Path $dist 'NS09-Native-Rumble-Xbox360-v1.0.0-public.zip'
if(Test-Path -LiteralPath $zip){Remove-Item -LiteralPath $zip}
Add-Type -AssemblyName System.IO.Compression
$archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
function Add-PublicFile([string]$path,[string]$name){
 $entry=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$path,$name.Replace('\','/'))
 $entry.ExternalAttributes=if($name.EndsWith('.sh')-or $name-eq 'bin/ns09-bridged'){[int]0x81ED -shl 16}elseif($name-eq 'bin/framework-probe.dex'){[int]0x8124 -shl 16}else{[int]0x81A4 -shl 16}
}
try{
 foreach($file in Get-ChildItem -LiteralPath $module -Recurse -File){Add-PublicFile $file.FullName ([IO.Path]::GetRelativePath($module,$file.FullName).Replace('\','/'))}
 foreach($name in 'README.md','README-VI.md','CHANGELOG.md','LICENSE','THIRD-PARTY-NOTICES.md'){Add-PublicFile (Join-Path $root $name) $name}
 foreach($file in Get-ChildItem -LiteralPath (Join-Path $root 'licenses') -File){Add-PublicFile $file.FullName ('licenses/'+$file.Name)}
}finally{$archive.Dispose()}
$archive=[IO.Compression.ZipFile]::OpenRead($zip)
try{
 foreach($name in 'module.prop','service.sh','runner.sh','customize.sh','keylayout.sh','bin/ns09-bridged','bin/framework-probe.dex','README.md','README-VI.md','LICENSE'){
  $entry=$archive.GetEntry($name);if(-not $entry){throw ('Missing ZIP entry: '+$name)}
  if(($name.EndsWith('.sh')-or $name-eq 'bin/ns09-bridged')-and (($entry.ExternalAttributes-shr 16)-band 511)-ne 493){throw ('Missing executable mode: '+$name)}
  if($name-eq 'bin/framework-probe.dex'-and (($entry.ExternalAttributes-shr 16)-band 511)-ne 292){throw 'DEX mode is not read-only'}
 }
}finally{$archive.Dispose()}
$manifest=[ordered]@{Version='1.0.0';Distribution='public-preview';Zip=[IO.Path]::GetFileName($zip);Sha256=(Get-FileHash -LiteralPath $zip).Hash;Bytes=(Get-Item -LiteralPath $zip).Length;OriginalVersionInUseUserConfirmed='1.0.0 Xbox360 Edition';NativeRumbleUserReported=$true;PublicIdentityAdaptationDeviceTested=$false;MotionGyroSupported=$false;HostTests=$tests;ElfStaticArm64Alignment16KiB=$true;ShellAndZipModesPassed=$true;MotorAndFfBackendUnchanged=$true}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $dist 'build-manifest.json') -Encoding UTF8
((Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetFileName($zip)) | Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt') -Encoding ascii
$manifest | ConvertTo-Json -Depth 5
