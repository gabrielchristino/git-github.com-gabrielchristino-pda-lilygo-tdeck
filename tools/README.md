# Ferramentas do PDA-OS

Execute a partir de `pda-os`. Testes ficam em `tests/`; resultados, caches e
experimentos locais ficam nas pastas ignoradas `build/` e `.pio-tools/`.
Há um único firmware padrão, `tdeck`, com Assistente incluído.

| Ferramenta | Finalidade |
| --- | --- |
| `flash.ps1`, `flash_device.py` | Compilar, conferir layout, fazer backup e gravar o aplicativo; migração explícita com `-MigratePartitions` |
| `device.py` | Console de diagnóstico e controle USB |
| `device_connection.py` | Transporte USB compartilhado pelos testes |
| `memory_report.py`, `profile_memory.py` | Medir ELF, heaps e reservas temporárias de PSRAM |
| `ai/check_assets.py` | Conferir pesos, corpus, manifesto e preparação usados pelo runtime |
| `ai/prepare_dataset.py` | Validar e congelar um corpus novo, separando treino, calibração e avaliação |
| `ai/train_pt.py` | Treinar e exportar o classificador PT; avaliação final fica separada |
| `ai/evaluate_pt.py` | Avaliar pesos congelados sem ajustar limiares |
| `ai/text_features.py`, `ai/policy.py` | Preparação lexical e política de capacidades atuais |
| `ai/generate_pt_folds.py` | Gerar as tabelas lexicais compartilhadas com o C++ |
| `ai/validate_pt.py` | Comparar características, probabilidades e classes Python/C++ |
| `ai/validate_pt_device.py` | Comparar reconhecedor do ESP32 com a referência, sem executar ações |
| `test_chat_device.py` | Consultas e prévias canceladas pela entrada do chat |
| `test_chat_crud_device.py` | CRUD de nota sintética identificável e timer, se não existir contagem em uso |
| `test_calendar_device.py` | Comparar datas com Python; pode sincronizar relógio inválido |
| `test_sound_device.py` | Testar áudio e timer; pode ativar Som e exige confirmação acústica humana |

```powershell
python tools/device.py --port COM4 status
python tools/device.py --port COM4 memory
python tools/device.py --port COM4 save
python tools/device.py --port COM4 bt-peers
python tools/device.py --port COM4 bt-log
python tools/device.py --port COM4 bt-manage
python tools/device.py --port COM4 bt-select --peer 'IDENTIDADE_MOSTRADA_EM_BT-PEERS'
python tools/memory_report.py
./tools/flash.ps1 -Port COM4 -SkipBuild
```

[Modelo, instalação das dependências e treinamento](../docs/IA.md) ·
[Aprendizados](../docs/APRENDIZADOS.md) · [Chat](../docs/CHAT.md) ·
[Partições e gravação](../docs/PARTICOES.md).

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
