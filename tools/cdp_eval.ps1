# Evaluates a JS expression in the first page target of nextchromium's DevTools port
# (nextchromium.ini debug_port) and prints the JSON result. Diagnostic tool.
param([string]$Expression, [int]$Port = 9222)
$ErrorActionPreference = 'Stop'
$targets = Invoke-RestMethod "http://127.0.0.1:$Port/json"
$page = $targets | Where-Object type -eq 'page' | Select-Object -First 1
if (-not $page) { throw 'no page target' }
"target: $($page.url.Substring(0, [Math]::Min(90, $page.url.Length)))"
$ws = [System.Net.WebSockets.ClientWebSocket]::new()
$ws.ConnectAsync([Uri]$page.webSocketDebuggerUrl, [Threading.CancellationToken]::None).Wait()
$msg = @{ id = 1; method = 'Runtime.evaluate'; params = @{ expression = $Expression; returnByValue = $true } } | ConvertTo-Json -Compress -Depth 5
$bytes = [Text.Encoding]::UTF8.GetBytes($msg)
$ws.SendAsync([ArraySegment[byte]]::new($bytes), 'Text', $true, [Threading.CancellationToken]::None).Wait()
$buf = New-Object byte[] 65536
$sb = [Text.StringBuilder]::new()
do {
    $r = $ws.ReceiveAsync([ArraySegment[byte]]::new($buf), [Threading.CancellationToken]::None).Result
    [void]$sb.Append([Text.Encoding]::UTF8.GetString($buf, 0, $r.Count))
} while (-not $r.EndOfMessage)
$sb.ToString()
