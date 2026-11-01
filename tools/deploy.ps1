# Installs the staged mod (tools\stage.ps1 output) into the game folder, or removes it.
# Only adds files: nextchromium.asi and the nextchromium\ folder. Original game files are never touched.
param(
    [string]$Game = 'C:\games\CS 1.6 - NextClient',
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
$out = Join-Path $PSScriptRoot '..\out'
if ($Remove) {
    Remove-Item (Join-Path $Game 'nextchromium.asi') -ErrorAction SilentlyContinue
    Remove-Item (Join-Path $Game 'nextchromium') -Recurse -Force -ErrorAction SilentlyContinue
    "removed nextchromium from $Game"
    return
}
Copy-Item (Join-Path $out 'nextchromium.asi') $Game -Force
$dst = Join-Path $Game 'nextchromium'
New-Item -ItemType Directory -Force $dst | Out-Null
Copy-Item (Join-Path $out 'nextchromium\nextchromium.dll') $dst -Force
Copy-Item (Join-Path $out 'nextchromium\cef') $dst -Recurse -Force
if (-not (Test-Path (Join-Path $dst 'nextchromium.ini'))) { Copy-Item (Join-Path $out 'nextchromium\nextchromium.ini') $dst }
"installed nextchromium into $Game"
