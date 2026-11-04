# Referências e dependências de terceiros

## Dados cartográficos

`src/core/map_data.inc` é uma extração simplificada e recortada de dados de
© OpenStreetMap contributors, distribuída sob a Open Database License 1.0
(ODbL): <https://www.openstreetmap.org/copyright>.
Termos completos: <https://opendatacommons.org/licenses/odbl/1-0/>.
Metadados, consulta de extração e hash do arquivo-fonte estão em
`data/maps/pinheiros.json`. `tools/maps/build_pinheiros.py` reproduz a conversão.
A atribuição e a referência da licença ficam na ajuda do mapa, pela tecla H.
Os arquivos de geometria são dados vetoriais, não imagens ou tiles raster.

`src/core/map_addresses.inc` contém pontos de apoio derivados do cadastro
público do **IBGE, CNEFE do Censo Demográfico 2022**, arquivo municipal de
São Paulo em GeoJSON, edição de 10/09/2024:
<https://ftp.ibge.gov.br/Cadastro_Nacional_de_Enderecos_para_Fins_Estatisticos/Censo_Demografico_2022/Arquivos_CNEFE/GeoJSON/Municipio_20240910/>.
A consulta é uma estimativa calculada pelo PDA, não uma localização exata
certificada pelo IBGE. Fonte, recorte, SHA-256 e validação estão em
`data/maps/pinheiros-interpolation-analysis.json` e `docs/ENDERECOS-APROXIMADOS.md`.
A ajuda do mapa identifica a fonte IBGE CNEFE 2022.

## Código e fontes

Este projeto inclui a definição de placa em `boards/` e as bibliotecas em `lib/`,
copiadas da referência local `t-deck-os`, com configurações e avisos preservados.
A licença da referência está em [licenses/t-deck-os-MIT.txt](licenses/t-deck-os-MIT.txt).

- TFT_eSPI: `lib/TFT_eSPI`, licença em `lib/TFT_eSPI/license.txt`.
- LVGL 8.4.0: `lib/lvgl`, licença em `lib/lvgl/LICENCE.txt`;
  `include/lv_conf_pda.h` reutiliza `lib/lv_conf.h` e desativa componentes não usados pelo PDA.
- SensorsLib: `lib/SensorsLib`, licença em `lib/SensorsLib/LICENSE`.
- NimBLE-Arduino 1.4.3: dependência gerenciada pelo PlatformIO.
- Arduino-ESP32 e ferramentas: dependências gerenciadas pelo PlatformIO.
- TinyGPSPlus 1.1.0: código em `vendor/TinyGPSPlus`, copyright Mikal Hart,
  LGPL 2.1 ou posterior; avisos preservados nos fontes.
  Projeto: <https://github.com/mikalhart/TinyGPSPlus>.
- `src/ui/font_pt.c`: glifos rasterizados de DejaVu Sans, com suporte a português.
  A licença e os avisos da fonte estão em [licenses/DejaVu.txt](licenses/DejaVu.txt).
  `tools/generate_font.ps1` reproduz a geração no Windows com DejaVu Sans instalada.

A sequência de inicialização e a pinagem de `src/hal/tdeck.cpp` foram adaptadas
das referências locais e dos exemplos oficiais LilyGO T-Deck:
<https://github.com/Xinyuan-LilyGO/T-Deck>.

Avisos das referências usadas:

Copyright (c) 2023 Shenzhen Xin Yuan Electronic Technology Co., Ltd

Copyright (c) 2026 zobithecat

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
