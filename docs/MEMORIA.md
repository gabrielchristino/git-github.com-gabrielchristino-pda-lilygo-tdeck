# Uso de memória e compactação

[Aprendizados de memória e IA local](APRENDIZADOS.md), incluindo
reservas verificadas de 2,5–4,5 MiB e os custos do mapa e salvamento.

**Atualização de 04/11/2026:** o [layout sem OTA](PARTICOES.md) amplia a partição
do aplicativo para 13.107.200 bytes (12,5 MiB). A compilação atual usa 5.448.133
bytes (41,6%), com 7.659.067 bytes livres. As medições abaixo são históricas,
anteriores à mudança de partições.

Medição de 03/11/2026, ambiente PlatformIO `tdeck`, mesma partição de aplicativo
de 6.553.600 bytes. O percentual da compilação corresponde à partição do
aplicativo, não a toda a flash de 16 MB e nem à RAM do ESP32-S3.

| Medida | Original | Primeira rodada | Segunda rodada |
| --- | ---: | ---: | ---: |
| Aplicativo, seções medidas pelo PlatformIO | 6.517.669 bytes | 5.583.665 bytes | 5.204.969 bytes |
| Ocupação da partição | 99,5% | 85,2% | 79,4% |
| Espaço livre na partição | 35.931 bytes | 969.935 bytes | 1.348.631 bytes |
| RAM interna estática | 37.872 bytes | 37.648 bytes | 37.648 bytes |
| Imagem `firmware.bin` | 6.518.032 bytes | 5.584.032 bytes | 5.205.328 bytes |

Economia líquida: **1.312.700 bytes**, sendo 378.696 na segunda rodada,
incluindo o comando USB `PDA HOME` usado na validação física.
A RAM estática não inclui heap em execução,
pilhas de tarefas ou PSRAM; esses valores precisam ser observados no aparelho.
O layout de partições e o formato dos documentos pessoais permanecem iguais.

## O que foi reduzido

Os dados offline do mapa eram o maior custo. Mantivemos o recorte, a geometria,
os nomes, os CEPs, as faixas e a precisão das coordenadas. Não houve nova
simplificação, redução da cobertura ou transferência da base inteira para RAM.

| Tabela na flash | Antes | Depois |
| --- | ---: | ---: |
| 62.283 segmentos do mapa | 1.183.377 | 996.528 |
| Referências do índice espacial | 274.792 | 137.396 |
| 20.846 registros de ruas CNEFE | 250.152 | 166.768 |
| 70.182 faixas numéricas | 561.456 | 421.092 |
| 223.204 apoios numéricos | 1.339.224 | 1.015.208 |
| Nomes OSM, incluindo índice e descritor compactos | 449.650 | 268.602 |
| Nomes CNEFE, incluindo índice e descritor compactos | 446.256 | 247.610 |

Os deslocamentos usam o tipo compartilhado `FlashOffset24`, com três bytes;
contagens e identificadores usam o menor inteiro que comporta esta base. Os
geradores rejeitam dados maiores que os limites, em vez de truncá-los.

Os apoios guardam diferenças entre números e coordenadas, com inteiros de
comprimento variável e zigzag para sinais. Cada faixa começa em um ponto de
reinício independente. A consulta decodifica somente a faixa candidata, com
memória temporária constante, sem biblioteca adicional de descompressão. A
busca no interior da faixa passa de binária para sequencial; esta base tem no
máximo 52 apoios por faixa. O tempo de consulta no hardware ainda precisa ser
medido nesta versão.

Na segunda rodada, `FlashStrings` e o gerador compartilhado `flash_strings.py`
passaram a atender os nomes OSM e CNEFE. Nomes ordenados compartilham prefixos
em blocos de 16, preservando bytes UTF-8, grafia e acentos. Acesso aleatório
decodifica no máximo um bloco; buscas sequenciais reutilizam o nome anterior
em um cursor local. O código não mantém um buffer global que possa ser
sobrescrito por outra consulta.

A busca de ruas normaliza cada nome único uma vez, reutilizando a correspondência
em todos os segmentos daquela rua. Uma versão inicial que repetia a decodificação
por segmento ficou mais lenta e foi substituída antes de fechar a mudança.
Na comparação local com `g++ -O2`, 40 buscas levaram 747.238 µs na primeira
rodada e 277.296 µs na segunda, com o mesmo checksum de resultados (5.162.952).
Isso é uma amostra no computador; o tempo no ESP32-S3 precisa de medição física.

O LVGL agora usa `include/lv_conf_pda.h`, compartilhado com o simulador. Ele
reutiliza as cores, fontes, tema, alocador e temporização da referência e
desativa componentes não usados, como chart, teclado virtual, tabela nativa,
calendário nativo, GIF e temas alternativos. Os aplicativos Calendário e
Planilha continuam usando seus próprios controles. Nenhum aplicativo foi
removido e os arquivos do projeto de referência não foram editados.

## Reproduzir e conferir

```powershell
python tools/maps/build_pinheiros.py
python tools/maps/analyze_interpolation.py --export build/maps/pinheiros-interpolation-50m.bin
./tests/run.ps1
cmake --build build/preview --parallel 6
Push-Location build/preview
./pda_preview.exe
Pop-Location
pio run -e tdeck
python tools/memory_report.py --baseline-flash 6517669
g++ -O2 -std=c++11 -I include tools/maps/benchmark_search.cpp src/core/map.cpp -o build/search_benchmark.exe
./build/search_benchmark.exe
```

Os geradores usam os arquivos locais de origem já descritos nas licenças do
mapa. Não alteram a tolerância de 50 m do modelo CNEFE. O binário intermediário
PDAINT02 continua no formato original; a compactação acontece na exportação
para C++.

`memory_report.py` lê o ELF com as ferramentas da toolchain, usa as mesmas
seções da medição do PlatformIO, lê a capacidade em `partitions.bin` e lista os
maiores símbolos. `--json` permite guardar o resultado para comparação futura.
O script não acessa a porta serial.

Validação concluída no computador:

- Suíte de Bluetooth, mapa, GPS, entradas, persistência e jogos aprovada.
- `map_encoding_test.cpp` verifica as impressões digitais dos nomes de todos
  os segmentos OSM e ruas CNEFE, todos os apoios contra a base original e
  5.786.619 interpolações contra a fórmula anterior. Também cobre leitura
  sequencial e aleatória, limites dos blocos e registros inválidos.
- Os exportadores conferem cada nome, número e coordenada após compactar e decodificar.
- Simulador LVGL aprovado, incluindo os testes de alertas Bluetooth e GPS.
- Capturas de notas, contatos, planilha, calendário, desenho, calculadora,
  2048 e mapa inicial idênticas aos PNGs de referência existentes.
- Na segunda rodada, capturas do mapa parado/em movimento, da busca de ruas,
  dos resultados de endereço e do marcador aproximado idênticas às da primeira.
- Compilação ESP32-S3 aprovada. SHA-256 da imagem:
  `28a89d41e4b28ebcb683a6c967aa397025a15ade726af6b538d6936012f520bf`.

## Outras possibilidades e seus custos

Os pontos da geometria ainda ocupam 825.404 bytes. Compactar suas coordenadas
é outro alvo possível, mas exige adaptar a montagem dos vetores e medir o
custo de decodificação durante movimento e zoom. A medição de geometrias
inteiras idênticas encontrou apenas 8 bytes reaproveitáveis.

Mover bases para SD mudaria o requisito de funcionamento sem cartão. Reduzir
o recorte ou simplificar mais as linhas e apoios mudaria cobertura ou precisão.
A ampliação da partição já foi realizada no [layout sem OTA](PARTICOES.md);
ela libera margem para o aplicativo, sem reduzir o binário.

A imagem foi instalada na COM4 com hash verificado. A validação por USB e
seus limites estão em [Validação no T-Deck](VALIDACAO-MEMORIA.md).
