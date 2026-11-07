param(
    [Parameter(Mandatory = $true)][string]$Puerto,
    [string]$Salida = "pruebas/traza_P10_placa_$(Get-Date -Format yyyyMMdd_HHmmss).txt",
    [ValidateRange(0,60)][int]$Espera = 8
)
$serial = [System.IO.Ports.SerialPort]::new($Puerto, 115200, 'None', 8, 'One')
$serial.WriteTimeout = 1000
$clock = [System.Diagnostics.Stopwatch]::new()
$log = [System.Collections.Generic.List[string]]::new()
$log.Add("P10 | Puerto: $Puerto | Fecha: $(Get-Date -Format o)")
$log.Add('TX/RX_ms son tiempos del PC. lat_us proviene de CTIMER1 en la placa.')
$pending = ''
$sent = 0
$done = $false
$modeChanges = 0
try {
    $serial.Open()
    Write-Host "P10 empieza en $Espera s. Durante 30 s gira el potenciometro. El script alternara el modo por UART."
    Start-Sleep -Seconds $Espera
    $serial.DiscardInBuffer()
    $clock.Start()
    while ($clock.Elapsed.TotalMilliseconds -lt 38000 -and -not $done) {
        $now = $clock.Elapsed.TotalMilliseconds
        if ($sent -lt 150 -and $now -ge $sent * 200) {
            $serial.Write("STATUS`n")
            ++$sent
            $log.Add(('TX_ms={0:F3} n={1} STATUS' -f $now, $sent))
        }
        if ($modeChanges -lt 5 -and $now -ge (5100 + $modeChanges * 5000)) {
            $command = if ($modeChanges % 2 -eq 0) { 'STOP' } else { 'START' }
            $serial.Write("$command`n")
            $log.Add(('TX_ms={0:F3} {1}' -f $now, $command))
            ++$modeChanges
        }
        $pending += $serial.ReadExisting()
        while (($split = $pending.IndexOf("`n")) -ge 0) {
            $line = $pending.Substring(0,$split).TrimEnd("`r")
            $pending = $pending.Substring($split+1)
            if ($line.Length -gt 0) {
                $log.Add(('RX_ms={0:F3} {1}' -f $clock.Elapsed.TotalMilliseconds,$line))
            }
            if ($line -eq 'P10 END') { $done = $true }
        }
        Start-Sleep -Milliseconds 2
    }
}
finally {
    $serial.Dispose()
    $log | Set-Content -LiteralPath $Salida -Encoding utf8
}
if ($sent -ne 150 -or -not $done) {
    throw "Captura incompleta: $sent envios. Traza: $Salida"
}
$latencies = @($log | ForEach-Object {
    if ($_ -match ' P10 RX=\d+ lat_us=(\d+)$') { [int]$Matches[1] }
})
if ($latencies.Count -ne 150) {
    throw "Se esperaban 150 mediciones. Llegaron $($latencies.Count). Traza: $Salida"
}
$summary = $latencies | Measure-Object -Minimum -Maximum
Write-Host "P10: $($summary.Count) respuestas medidas, min=$($summary.Minimum) us, max=$($summary.Maximum) us. Traza: $Salida"
