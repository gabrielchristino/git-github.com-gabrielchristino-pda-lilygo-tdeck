param([string]$Port = '', [switch]$SkipBuild)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$pythonPath = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$esptoolPath = Join-Path $env:USERPROFILE '.platformio/packages/tool-esptoolpy/esptool.py'
if (!(Test-Path -LiteralPath $pythonPath) -or !(Test-Path -LiteralPath $esptoolPath)) { throw 'Instale PlatformIO antes de usar este script.' }
Push-Location -LiteralPath $projectRoot
try {
    if (!$SkipBuild) {
        & $pythonPath -m platformio run -e tdeck
        if ($LASTEXITCODE -ne 0) { throw 'Falha na compilação; nenhum firmware foi gravado.' }
    }
    if (!$Port) {
        $devicePorts = & $pythonPath -c 'import json; from serial.tools import list_ports; print(json.dumps([p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]))'
        $serialMatches = @($devicePorts | ConvertFrom-Json)
        if ($serialMatches.Count -ne 1) { throw 'Informe -Port COMx: é necessário identificar exatamente um T-Deck.' }
        $Port = $serialMatches[0]
    }
    $firmwarePath = Join-Path $projectRoot '.pio/build/tdeck/firmware.bin'
    if (!(Test-Path -LiteralPath $firmwarePath)) { throw 'Firmware não encontrado.' }
    # Application only. Keep the existing bootloader, partition map and PDA files.
    & $pythonPath $esptoolPath --chip esp32s3 --port $Port --baud 921600 --before usb_reset --after hard_reset --connect-attempts 3 write_flash -z --flash_mode keep --flash_freq keep --flash_size keep 0x10000 $firmwarePath
    if ($LASTEXITCODE -ne 0) { throw 'Falha na gravação.' }
} finally { Pop-Location }
