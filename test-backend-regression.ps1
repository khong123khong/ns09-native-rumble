[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$source=Join-Path $PSScriptRoot 'bridge-src'
$baseline=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'fixtures/backend-regression.json') -Raw | ConvertFrom-Json
foreach($entry in $baseline.Files.PSObject.Properties){
 if((Get-FileHash -LiteralPath (Join-Path $source $entry.Name)).Hash-ne $entry.Value){throw ('Rumble/backend regression: '+$entry.Name)}
}
$code=[IO.File]::ReadAllText((Join-Path $source 'ns09_bridge.c')).Replace("`r`n","`n")
$parts=@(@('hid-writer-watchdog','typedef ns09_writer_message writer_message;','typedef struct { int fd; struct uinput_ff_upload native; } upload_adapter;'),@('ff-event-lifecycle','static int ff_event(','static int run_connection('),@('session-lifecycle','static int run_connection(','int main('))
foreach($part in $parts){
 $start=$code.IndexOf($part[1]);$end=$code.IndexOf($part[2],$start+1)
 if($start-lt 0-or $end-lt 0){throw ('Missing protected fragment: '+$part[0])}
 $sha=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($code.Substring($start,$end-$start))))
 if($sha-ne $baseline.Fragments.($part[0])){throw ('Protected production path changed: '+$part[0])}
}
'PASS: original rumble/Stop/FF/writer/watchdog/session and guardian are unchanged; public identity redaction is separate.'
