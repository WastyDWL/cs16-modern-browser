# Builds out\nextchromium-<version>.zip from the staged mod (run tools\stage.ps1 first).
param([string]$Version = (Get-Date -Format 'yyyy.MM.dd'))
$ErrorActionPreference = 'Stop'
$out = Resolve-Path (Join-Path $PSScriptRoot '..\out')
$pkg = Join-Path $out 'package'
Remove-Item $pkg -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory $pkg | Out-Null
Copy-Item (Join-Path $out 'nextchromium.asi') $pkg
Copy-Item (Join-Path $out 'nextchromium') $pkg -Recurse
Get-ChildItem (Join-Path $pkg 'nextchromium') -Include *.log, harness.log -Recurse | Remove-Item -Force
Remove-Item (Join-Path $pkg 'nextchromium\cache') -Recurse -Force -ErrorAction SilentlyContinue
Copy-Item (Join-Path $PSScriptRoot '..\README.md') $pkg
$zip = Join-Path $out "nextchromium-$Version.zip"
Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $pkg '*') -DestinationPath $zip
"$zip  ($([Math]::Round((Get-Item $zip).Length / 1MB)) MB)"
