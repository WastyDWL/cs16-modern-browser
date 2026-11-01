# Assembles the mod as it is installed into the game folder:
#   out\nextchromium.asi
#   out\nextchromium\nextchromium.dll
#   out\nextchromium\cef\  (CEF runtime + resources + ncb_subprocess.exe)
param([string]$Out = (Join-Path $PSScriptRoot '..\out'))
$ErrorActionPreference = 'Stop'
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$build = Join-Path $root 'build'
$cefDist = Join-Path $root 'third_party\cef'

$mod = Join-Path $Out 'nextchromium'
$cef = Join-Path $mod 'cef'
New-Item -ItemType Directory -Force $cef | Out-Null

Copy-Item (Join-Path $build 'src\loader\nextchromium.asi') $Out -Force
Copy-Item (Join-Path $build 'src\core\nextchromium.dll') $mod -Force
Copy-Item (Join-Path $build 'src\subprocess\ncb_subprocess.exe') $cef -Force
Get-ChildItem (Join-Path $cefDist 'Release') -File |
    Where-Object { $_.Extension -in '.dll', '.bin', '.json' } |
    Copy-Item -Destination $cef -Force
Copy-Item (Join-Path $cefDist 'Resources\*') $cef -Recurse -Force
$ini = Join-Path $mod 'nextchromium.ini'
if (-not (Test-Path $ini)) {
    Set-Content -Encoding ascii $ini @"
[main]
; 0 disables nextchromium (the game uses its original browser). -nonextchromium does the same.
enabled=1
; frames per second the pages are rendered at (1-240)
frame_rate=60
; page scale in percent (25-300); 90 fits typical MOTD pages into the 720px window
zoom=90
; 1 = shrink pages wider than the window (fixed-width MOTD tables) so they fit without scrolling
fit_width=1
; Chrome DevTools at http://127.0.0.1:<port> (0 = off)
debug_port=0
"@
}
"staged to $(Resolve-Path $Out)"
