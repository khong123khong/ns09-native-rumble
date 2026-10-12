[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$cache=if($env:NS09_TOOL_ROOT){$env:NS09_TOOL_ROOT}else{[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../.ns09-tools'))}
$bb=Join-Path $cache 'busybox64.exe'
$module=Join-Path $PSScriptRoot 'bridge-module'
if(-not(Test-Path -LiteralPath $module)){$module=Split-Path -Parent $PSScriptRoot}
$original=[IO.File]::ReadAllText((Join-Path $module 'runner.sh'))
$utf8=[Text.UTF8Encoding]::new($false)
$results=@()
foreach($scenario in 'three-failures','clean-exit','disabled-before-boot'){
 $folder=Join-Path $PSScriptRoot ('build\shell-tests\'+$scenario)
 New-Item -ItemType Directory -Path $folder -Force | Out-Null
 foreach($name in 'bridge.off','supervisor.pid','calls.txt'){$path=Join-Path $folder $name;if(Test-Path -LiteralPath $path){Remove-Item -LiteralPath $path}}
 $boot=if($scenario-eq 'disabled-before-boot'){0}else{1}
 $exit=if($scenario-eq 'three-failures'){7}else{0}
 $mocks="`ngetprop() { printf '$boot'; }`nsleep() { :; }`nflock() { :; }`nns09_fake_daemon() { printf 'called\n' >> `"`$MODDIR/calls.txt`"; return $exit; }`n"
 # Execute original supervisor control flow in ash; replace only platform commands.
 $script=$original.Replace('umask 077','umask 077'+$mocks).Replace('timeout 8 sh "$MODDIR/diagnose.sh" 2>/dev/null',':').Replace('"$MODDIR/bin/ns09-bridged" --recover',':').Replace('"$MODDIR/bin/ns09-bridged" --module "$MODDIR"','ns09_fake_daemon --module "$MODDIR"')
 [IO.File]::WriteAllText((Join-Path $folder 'runner.sh'),$script,$utf8)
 if($scenario-eq 'disabled-before-boot'){[IO.File]::WriteAllBytes((Join-Path $folder 'bridge.off'),[byte[]]@())}
 & $bb sh (Join-Path $folder 'runner.sh')
 if($LASTEXITCODE){throw "Supervisor mock failed: $scenario"}
 $callsPath=Join-Path $folder 'calls.txt'
 $calls=if(Test-Path -LiteralPath $callsPath){@(Get-Content -LiteralPath $callsPath).Count}else{0}
 $expected=if($scenario-eq 'three-failures'){3}elseif($scenario-eq 'clean-exit'){1}else{0}
 if($calls-ne $expected){throw "Unexpected restart count $scenario : $calls"}
 if(Test-Path -LiteralPath (Join-Path $folder 'supervisor.pid')){throw "PID cleanup failed: $scenario"}
 if($scenario-eq 'three-failures'-and (Get-Content -LiteralPath (Join-Path $folder 'bridge-status.txt') -Raw)-notmatch 'failed three times'){throw 'Missing crash status'}
 $results+=[pscustomobject]@{Scenario=$scenario;DaemonCalls=$calls;Expected=$expected;PidCleaned=$true;Pass=$true}
}
ConvertTo-Json -InputObject @($results) -Depth 4 | Set-Content -LiteralPath (Join-Path $PSScriptRoot 'build\startup-tests.json') -Encoding UTF8
$results | Format-Table
Write-Output 'PASS: supervisor control flow in ash using mocked Android/daemon/flock/sleep. This does not exercise Android uinput.'
