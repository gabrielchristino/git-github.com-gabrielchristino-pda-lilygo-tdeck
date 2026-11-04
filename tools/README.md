# Ferramentas do PDA-OS

Execute a partir de `pda-os`. As ferramentas de desenvolvimento ficam aqui;
testes de regressão ficam em `tests/`. Experimentos pontuais não fazem parte
das ferramentas mantidas. Os logs históricos continuam em `build/` e `backups/`.

## Uso cotidiano

| Ferramenta | Finalidade |
| --- | --- |
| `flash.ps1` | Compila e grava somente o aplicativo, preservando a área de documentos |
| `device.py` | Console único de diagnóstico e controle USB do T-Deck |
| `memory_report.py` | Mede o ELF e lista os maiores símbolos da firmware |

```powershell
python tools/device.py --port COM4 status
python tools/device.py --port COM4 save
python tools/device.py --port COM4 bt-peers
python tools/device.py --port COM4 bt-log
python tools/device.py --port COM4 bt-manage
python tools/device.py --port COM4 bt-select --peer 'IDENTIDADE_MOSTRADA_EM_BT-PEERS'
python tools/memory_report.py
./tools/flash.ps1 -Port COM4 -SkipBuild
```

Use a porta real e uma identidade copiada de `bt-peers`. `bt-manage` e
`bt-select` exigem o app Bluetooth aberto. `restart` reinicia o **T-Deck**.
Estas ferramentas não reiniciam o dongle Bluetooth/Wi-Fi do computador.

`device.py` requer pyserial. Use um ambiente Python com essa dependência instalada:

```powershell
python tools/device.py --help
```

## Testes e reprodução dos recursos

- `../tests/run.ps1`: regras, persistência, entradas, GPS, mapas e política de destinos BLE.
- `preview/`: simulador LVGL, auditoria de fontes e exportação das capturas.
- `test_bluetooth_pc.py` e `BluetoothReceiver.cs`: teste de recepção HID no Windows,
  com a janela receptora ativa e focada; não são necessários para usar o teclado.
- `generate_font.ps1` e `generate_game_levels.py`: geradores das fontes e fases.
- `maps/build_pinheiros.py`: gera os vetores offline a partir do extrato OSM local.
- `maps/analyze_addresses.py` e `maps/analyze_interpolation.py`: análise e exportação CNEFE.
- `maps/flash_strings.py`: módulo compartilhado de compactação dos nomes; usado pelos geradores.
- `maps/benchmark_search.cpp`: comparativo de busca no computador, descrito em `../docs/MEMORIA.md`.
