param(
    [Parameter(Mandatory = $true)][string]$Puerto,
    [string]$Salida = "pruebas/traza_P02_captura_$(Get-Date -Format yyyyMMdd_HHmmss).txt"
)

$serial = [System.IO.Ports.SerialPort]::new($Puerto, 115200, 'None', 8, 'One')
$serial.NewLine = "`n"
$serial.ReadTimeout = 200
$clock = [System.Diagnostics.Stopwatch]::StartNew()
$log = [System.Collections.Generic.List[string]]::new()
$log.Add("P02 | Puerto: $Puerto | Fecha: $(Get-Date -Format o)")
$log.Add('PC_ms marca la lectura de la linea completa por el PC, via MCU-Link USB.')
$lastArrival = $null
$lastBoard = $null
$count = 0
$channels = @{}

try {
    $serial.Open()
    $serial.DiscardInBuffer()
    Write-Host 'Pulsa RESET en la placa ahora. No envies comandos durante la captura.'
    while ($clock.Elapsed.TotalSeconds -lt 35 -and ($count -lt 10 -or $channels.Count -lt 2)) {
        try {
            $line = $serial.ReadLine().TrimEnd("`r")
            $arrival = $clock.Elapsed.TotalMilliseconds
        }
        catch [System.TimeoutException] {
            continue
        }
        if ($line -match '^t=(\d+);ADC=\d+;PCT=\d+;THR=\d+;MODE=(RUN|PAUSE);ALARM=[01]$') {
            $board = [long]$Matches[1]
            if ($board -eq 1000 -or ($count -gt 0 -and $count -lt 10 -and
                ($board -ne $lastBoard + 1000 -or $arrival - $lastArrival -lt 50))) {
                $log.RemoveRange(2, $log.Count - 2)
                $channels.Clear()
                $count = 0
                $lastArrival = $null
                $lastBoard = $null
            }
            if ($count -ge 10 -or ($count -eq 0 -and $board -ne 1000)) { continue }
            $entry = 'PC_ms={0:F3} | {1}' -f $arrival, $line
            if ($count -gt 0) {
                $entry += ' | delta_PC_ms={0:F3} | delta_t_ms={1}' -f ($arrival - $lastArrival), ($board - $lastBoard)
            }
            $lastArrival = $arrival
            $lastBoard = $board
            ++$count
        }
        elseif ($count -eq 10 -and $line -match '^P02 (ADC|LED) ventana_us=\d+ flancos=\d+ min_us=\d+ max_us=\d+$') {
            $channels[$Matches[1]] = $true
            $entry = $line
        }
        else {
            continue
        }
        $log.Add($entry)
        Write-Host $entry
    }
}
finally {
    $serial.Dispose()
    $log | Set-Content -LiteralPath $Salida -Encoding utf8
}
if ($count -ne 10 -or $channels.Count -ne 2) {
    throw "Captura incompleta: $count lineas de telemetria y $($channels.Count) resumenes. Traza: $Salida"
}
Write-Host "Traza guardada en $Salida"
