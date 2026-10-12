[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$cache=if($env:NS09_TOOL_ROOT){$env:NS09_TOOL_ROOT}else{[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../.ns09-tools'))}
$taskBb=Join-Path $cache 'busybox64.exe'
$taskOriginal=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'bridge-module/keylayout.sh') -Raw
$taskAsset=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'bridge-module/assets/Xbox360.kl') -Raw
$taskTemp=Join-Path $env:TEMP 'ns09-xbox-keylayout-tests'
if($taskTemp.Contains(' ')){throw 'Keylayout mock test requires a temporary path without spaces.'}
$taskResults=@()
$taskUtf8=[Text.UTF8Encoding]::new($false)
foreach($taskCase in 'stock','missing','wrong-axis','decimal'){
 $taskFolder=Join-Path $taskTemp $taskCase
 $taskModule=Join-Path $taskFolder 'module'
 $taskRom=Join-Path $taskFolder 'rom'
 New-Item -ItemType Directory -Path (Join-Path $taskModule 'assets'),$taskRom -Force | Out-Null
 $taskSkip=Join-Path $taskModule 'skip_mount'
 [IO.File]::WriteAllBytes($taskSkip,[byte[]]@())
 [IO.File]::WriteAllText((Join-Path $taskModule 'assets/Xbox360.kl'),$taskAsset,$taskUtf8)
 $taskStock=Join-Path $taskRom 'Vendor_045e_Product_028e.kl'
 if(Test-Path -LiteralPath $taskStock){Remove-Item -LiteralPath $taskStock}
 if($taskCase-ne 'missing'){
   $taskFixture=if($taskCase-eq 'wrong-axis'){$taskAsset.Replace('axis 0x03 Z','axis 0x03 RX')}elseif($taskCase-eq 'decimal'){$taskAsset.Replace('0x00','0').Replace('0x01','1').Replace('0x02','2').Replace('0x03','3').Replace('0x04','4').Replace('0x05','5').Replace('0x10','16').Replace('0x11','17')}else{$taskAsset}
   [IO.File]::WriteAllText($taskStock,$taskFixture,$taskUtf8)
 }
 $taskScript=Join-Path $taskFolder 'test.sh'
 $taskPrefix='MODPATH='''+$taskModule.Replace('\','/')+"'`n"+'NS09_KL_ROOTS='''+$taskRom.Replace('\','/')+"'`n"
 [IO.File]::WriteAllText($taskScript,$taskPrefix+$taskOriginal+"`nns09_select_keylayout || exit 1`n",$taskUtf8)
 & $taskBb sh $taskScript
 if($LASTEXITCODE){throw "Keylayout selection mock failed: $taskCase"}
 $taskFallback=Join-Path $taskModule 'system/usr/keylayout/Vendor_045e_Product_028e_Version_0001.kl'
 $taskExpected=$taskCase-in 'missing','wrong-axis'
 if((Test-Path -LiteralPath $taskSkip)-eq $taskExpected){throw "Wrong skip_mount behavior: $taskCase"}
 if($taskExpected-and -not(Test-Path -LiteralPath $taskFallback)){throw "Missing conditional fallback: $taskCase"}
 if(Get-ChildItem -LiteralPath $taskModule -Recurse -Filter Generic.kl){throw 'Generic.kl must not be overlaid'}
 if(Get-ChildItem -LiteralPath $taskModule -Recurse -Filter '*02e0*'){throw 'Physical NS09 keylayout must not be overlaid'}
 $taskResults+=[pscustomobject]@{Case=$taskCase;Pass=$true;Fallback=$taskExpected;GenericUntouched=$true;Physical02e0Untouched=$true}
}
$taskResults | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build/xbox-keylayout-tests.json') -Encoding UTF8
$taskResults | Format-Table
Write-Output 'PASS: production keylayout selection in ash; existing ROM reuse, missing/incompatible fallback, decimal/hex semantics; no Generic/physical overlay.'
