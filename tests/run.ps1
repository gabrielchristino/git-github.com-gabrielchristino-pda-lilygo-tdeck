$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $projectRoot
try {
    New-Item -ItemType Directory -Force build | Out-Null
    & g++ -std=c++11 -Wall -Wextra -Werror -I include tests/bluetooth_test.cpp src/core/bluetooth.cpp -o build/bluetooth_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes Bluetooth.' }
    & ./build/bluetooth_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes Bluetooth.' }
    & g++ -std=c++11 -Wall -Wextra -Werror -I tests/fakes_ble -I include -I src tests/bluetooth_peers_test.cpp src/hal/bluetooth.cpp src/core/bluetooth.cpp -o build/bluetooth_peers_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes dos destinos BLE.' }
    & ./build/bluetooth_peers_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes dos destinos BLE.' }
    & g++ -std=c++11 -Wall -Wextra -Werror -I include tests/map_test.cpp src/core/map.cpp src/core/map_addresses.cpp -o build/map_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes do mapa.' }
    & ./build/map_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes do mapa.' }
    & g++ -std=c++11 -Wall -Wextra -Werror -I include tests/map_encoding_test.cpp src/core/map.cpp -o build/map_encoding_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes da compactação do mapa.' }
    & ./build/map_encoding_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha na equivalência dos dados compactados.' }
    # TinyGPSPlus intentionally falls through from comma to term terminators.
    & g++ -std=c++11 -Wall -Wextra -Werror -Wno-implicit-fallthrough -DARDUINO=10800 -I tests/fakes -I src -I include -I vendor/TinyGPSPlus/src tests/gps_test.cpp src/hal/gps.cpp src/core/datetime.cpp vendor/TinyGPSPlus/src/TinyGPS++.cpp -o build/gps_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes GPS.' }
    & ./build/gps_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes GPS.' }
    & g++ -std=c++11 -Wall -Wextra -Werror -I include tests/input_test.cpp src/core/input.cpp -o build/input_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes de entrada.' }
    & ./build/input_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes de entrada.' }
    & g++ -std=c++11 -Wall -Wextra -Werror -I include tests/core_test.cpp src/core/model.cpp src/core/datetime.cpp src/core/storage.cpp src/core/expression.cpp src/core/system.cpp src/core/games.cpp src/core/more_games.cpp -o build/core_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes do sistema.' }
    & ./build/core_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes do sistema.' }
    & g++ -std=c++11 -Wall -Wextra -Werror -I include tests/games_test.cpp tests/more_games_test.cpp src/core/games.cpp src/core/more_games.cpp src/core/model.cpp src/core/storage.cpp src/core/datetime.cpp -o build/games_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar testes dos jogos.' }
    & ./build/games_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Falha nos testes dos jogos.' }
} finally {
    Pop-Location
}
