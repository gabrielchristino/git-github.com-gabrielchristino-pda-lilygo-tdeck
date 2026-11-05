# Flash sem OTA

O arquivo `partitions_no_ota.csv`, selecionado pela definição de placa, reserva
uma única aplicação `factory` em `0x10000`, com 13.107.200 bytes (12,5 MiB).
Substitui os dois slots OTA antigos de 6,25 MiB cada. O bootloader existente
suporta a aplicação factory; não é necessário regravá-lo.

| Área | Endereço | Tamanho |
| --- | --- | --- |
| Tabela | 0x8000 | setor de 4 KiB |
| NVS | 0x9000 | 20 KiB |
| Espaço não utilizado, antigos metadados OTA | 0xe000 | 8 KiB |
| Firmware factory | 0x10000 | 12,5 MiB |
| Documentos LittleFS, rótulo spiffs | 0xc90000 | 3,375 MiB |
| Coredump | 0xff0000 | 64 KiB |

NVS, LittleFS e coredump mantêm seus endereços, tamanhos, subtipos e rótulos.
O conteúdo antigo dos metadados OTA pode continuar nos 8 KiB não utilizados;
o novo layout não o referencia. Não é preciso apagar a flash inteira.

## Compilar e migrar

```powershell
python -m pip install --target .pio-tools/flash -r tools/requirements-flash.txt
./tools/flash.ps1 -Port COM4 -MigratePartitions
```

O esptool 4.8.1 fica isolado no projeto. A versão 4.5.1 incluída no PlatformIO
tem leitura USB muito lenta neste S3; o instalador de firmware não altera o
ambiente global nem a ferramenta usada na compilação. O Python do PlatformIO
pode ser usado no lugar de `python` se este não estiver no PATH.

O aparelho precisa estar executando o PDA para confirmar `PDA SAVE`. O script:

1. Compila e valida a tabela binária, checksum e tamanho da aplicação.
2. Solicita salvamento e lê a tabela instalada.
3. Aceita apenas o layout antigo conhecido ou o novo, recusando outros layouts.
4. Salva e verifica uma cópia dos 16 MiB em `backups/flash-DATA-HORA/`.
5. Grava somente a tabela em `0x8000` e o firmware em `0x10000`.
6. Verifica firmware, tabela, bootloader, configurações, documentos e coredump
   antes de reiniciar. As regiões preservadas são comparadas por digest com
   os bytes da cópia completa, enquanto o firmware ainda não está executando.

A pasta contém a imagem anterior, manifesto com SHA-256 e os binários novos.
Backups incluem dados pessoais e configurações; são ignorados pelo Git.
Após migrar, as atualizações normais usam `./tools/flash.ps1 -Port COM4`.
Sem a opção de migração, um layout divergente é recusado antes da escrita.

## Recuperação

Se ocorrer falha durante a escrita/verificação, a ferramenta mantém o aparelho
em modo de gravação e informa a pasta de recuperação. Corrija a causa antes de
prosseguir. Não use `erase_flash` nem grave um filesystem vazio.

Para restaurar integralmente a cópia anterior do mesmo aparelho, use esptool
do PlatformIO, substituindo porta e caminho pelo backup verificado:

```powershell
$pythonPath = Join-Path $env:USERPROFILE '.platformio/penv/Scripts/python.exe'
$esptoolPath = Join-Path $env:USERPROFILE '.platformio/packages/tool-esptoolpy/esptool.py'
& $pythonPath $esptoolPath --chip esp32s3 --port COM4 --before usb_reset --after hard_reset write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x0 'backups/flash-DATA-HORA/flash-before.bin'
```

Isso restaura também documentos e configurações ao instante do backup; mudanças
posteriores serão substituídas. É uma operação de recuperação, não atualização
cotidiana. Se o reset USB não responder, entre no modo de gravação físico da placa.

## Validação

`python tests/flash_layout_test.py` verifica checksum, layout desconhecido,
firmware maior que a partição, build antigo e exigência de migração explícita.
A compilação atual usa 5.222.645 bytes de aplicativo, com 7.884.555 bytes livres.

Migração física concluída em 04/11/2026, COM4:

- Backup completo de 16.777.216 bytes verificado contra a flash antes da escrita.
  Arquivo: `backups/flash-20261104-082204-917324/flash-before.bin`.
  SHA-256: `e528b8948b5c2f4a60f487cd76ed15c23f48baf5ac7782c53b2efde7276497db`.
- Tabela e aplicativo gravados e verificados. Bootloader, área de configurações
  e toda a região de documentos/coredump tiveram digest idêntico ao backup antes
  do reinício. Nenhuma área de documentos foi movida ou reformatada.
- Após reiniciar: `READY=1 STORAGE=1 DIRTY=0`; 1 nota, 1 contato, 3 compromissos,
  nenhuma planilha e nenhum desenho, como antes da migração.
- Os dez relatórios de jogos coincidiram integralmente com os anteriores.
- O destino Bluetooth salvo foi carregado ao abrir o app, reconectou e foi
  preservado ao voltar ao início. Antes de inicializar o Bluetooth, o console
  lista zero destinos porque o serviço ainda não carregou os vínculos.
- Compilação, sete testes do layout e suíte completa `tests/run.ps1` aprovados.
  Relatórios USB em `build/partition-validation-2026-11-04/`.

Imagem instalada: 5.223.008 bytes, SHA-256
`73e326bc35a5309883804a5bb25443a3ed3ee065f9f09c206d6bae2680beab2d`.

Referência oficial do formato e boot factory:
https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/partition-tables.html

Problema de desempenho da leitura USB nas ferramentas antigas:
https://github.com/espressif/esptool/issues/936
