param([string]$Port = '', [switch]$SkipBuild, [switch]$MigratePartitions)
$Environment = 'tdeck'
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$pythonPath = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$esptoolPath = Join-Path $env:USERPROFILE '.platformio/packages/tool-esptoolpy/esptool.py'
if (!(Test-Path -LiteralPath $pythonPath) -or !(Test-Path -LiteralPath $esptoolPath)) { throw 'Instale PlatformIO antes de usar este script.' }
Push-Location -LiteralPath $projectRoot
try {
    if (!$SkipBuild) {
        & $pythonPath -m platformio run -e $Environment
        if ($LASTEXITCODE -ne 0) { throw 'Falha na compilação; nenhum firmware foi gravado.' }
    }
    if (!$Port) {
        $devicePorts = & $pythonPath -c 'import json; from serial.tools import list_ports; print(json.dumps([p.device for p in list_ports.comports() if p.vid == 0x303A and p.pid == 0x1001]))'
        $serialMatches = @($devicePorts | ConvertFrom-Json)
        if ($serialMatches.Count -ne 1) { throw 'Informe -Port COMx: é necessário identificar exatamente um T-Deck.' }
        $Port = $serialMatches[0]
    }
    $firmwarePath = Join-Path $projectRoot ".pio/build/$Environment/firmware.bin"
    if (!(Test-Path -LiteralPath $firmwarePath)) { throw 'Firmware não encontrado.' }
    # Validate the installed layout before writing. Migration is explicit and backed up.
    $flashArguments = @('tools/flash_device.py', '--port', $Port, '--esptool', $esptoolPath, '--environment', $Environment)
    if ($MigratePartitions) { $flashArguments += '--migrate-partitions' }
    & $pythonPath @flashArguments
    if ($LASTEXITCODE -ne 0) { throw 'Falha na gravação.' }
} finally { Pop-Location }
