# Tamanho da busca por número — análise de 28/10/2026

A base de todo o recorte é maior que a estimativa inicial de 1–3 MB. Uma faixa
mais estreita perto do Rio Pinheiros cabe na partição atual do aplicativo.
Esta análise da base integral gerou arquivos no computador, sem instalar essa
base no aparelho. Posteriormente foi desenvolvida uma opção menor por
[interpolação aproximada](ENDERECOS-APROXIMADOS.md), usando pontos de apoio.

## Fonte e recorte

Fonte: [IBGE, CNEFE do Censo 2022, GeoJSON municipal publicado em setembro de 2024](https://ftp.ibge.gov.br/Cadastro_Nacional_de_Enderecos_para_Fins_Estatisticos/Censo_Demografico_2022/Arquivos_CNEFE/GeoJSON/Municipio_20240910/).
Foi baixado o município de São Paulo, código 3550308: 229.529.525 bytes de ZIP,
com 2.347.635.066 bytes de GeoJSON. Esses arquivos de origem ficam no computador.

Filtro: a mesma caixa do mapa, latitudes -23.755 a -23.520 e longitudes -46.810
a -46.590. Essa caixa abrange partes de municípios vizinhos, mas esta análise
inclui somente os endereços do município de São Paulo.

- 5.689.391 registros no arquivo municipal.
- 2.596.333 registros dentro da caixa do mapa.
- 2.543.518 registros com logradouro e número positivo.
- 981.735 combinações distintas de logradouro normalizado, texto completo do
  número e CEP, após reunir apartamentos/complementos do mesmo endereço.
- 965.523 dessas combinações têm coordenada original ou modificada, níveis de
  geocodificação 1 e 2. As demais usam estimativas ou referências de área.

Não se trata de uma contagem de entradas físicas verificadas. Permanecem
variantes cadastrais, e o endereço de 2022 pode ter mudado. Há 9.714 grupos em
que as coordenadas dos registros reunidos têm extensão diagonal superior a
100 metros; eles exigem revisão antes de uma importação que prometa precisão.
O protótipo conserva uma coordenada do melhor nível disponível, sem inventar
um número ou interpolar uma posição.

## Arquivos compactos medidos

O formato sem compressão usa registro de 16 bytes: referência ao logradouro,
referência ao texto do número, coordenadas locais e CEP/nível de geocodificação.
Nomes, números com modificadores e índices por logradouro também estão incluídos.

| Base | Combinações | Sem compressão | Protótipo com blocos comprimidos |
| --- | ---: | ---: | ---: |
| Todo o recorte, todos os níveis | 981.735 | 16,90 MB | 8,93 MB |
| Todo o recorte, somente níveis 1/2 | 965.523 | 16,62 MB | 8,80 MB |
| Até 1 km do Rio Pinheiros, níveis 1/2 | 34.010 | 625,59 KB | 373,55 KB |
| Até 2 km do Rio Pinheiros, níveis 1/2 | 103.650 | 1,84 MB | 1,04 MB |
| Até 3 km do Rio Pinheiros, níveis 1/2 | 178.654 | 3,14 MB | 1,76 MB |

MB e KB decimais. As faixas usam distância em linha reta para os segmentos
nomeados Rio Pinheiros no extrato OSM existente: cerca de 18,8 km de geometria,
com limites aproximados de latitude -23.6566 a -23.5253. **Não incluem uma
extensão ao longo do Jurubatuba ao sul**, nem representam toda a Zona Sul.
O mapa de fundo pode continuar com a cobertura maior; só a busca por número
ficaria limitada à faixa escolhida.

Os arquivos por blocos têm índices e blocos de até 128 registros, com buffer
de registros descomprimidos de no máximo 1.536 bytes, além da memória do
descompressor e da interface. Todos os blocos passaram por compressão e
descompressão com comparação de bytes no computador. O leitor no ESP32 ainda
não foi implementado ou medido; esses tamanhos não incluem o código do leitor.

Também foi medida compressão do arquivo inteiro: 6,84 MB para todo o recorte.
Esse número serve somente como comparação: não é um formato de pesquisa pronta
para o T-Deck e não permite descomprimir uma única faixa de registros.

## Encaixe na memória atual

Firmware medido antes da interpolação: **3.610.816 bytes**. Partição do aplicativo: **6.553.600 bytes**.
Disponível nessa partição: **2.942.784 bytes**.

A faixa de 2 km, sem compressão, ocupa 1.839.072 bytes. Somada ao firmware,
totaliza 5.449.888 bytes e deixa **1.103.712 bytes antes do novo código**.
É a opção mais direta para começar sem cartão SD. A faixa de 3 km cabe no
protótipo comprimido, mas exigiria desenvolver e validar o leitor de blocos.

Para todos os números de todo o recorte, a opção prática é uma base no microSD
ou um projeto específico de armazenamento. Esta análise não alterou partições
nem verificou a presença de um cartão SD no aparelho.

## Reprodução e evidências

- Ferramenta: `tools/maps/analyze_addresses.py`.
- Métricas e SHA-256: `data/maps/pinheiros-address-analysis.json`.
- Entradas e arquivos binários de análise: `build/maps/`.
- A conversão verifica cada registro serializado, seus índices de texto,
  coordenadas, CEP e nível. O ZIP é lido até o fim para validar o CRC.
- Consultas OSM de endereços em três instâncias Overpass não completaram
  (502/timeout). A contagem de endereços apresentada é do IBGE; o OSM existente
  foi usado somente como geometria do rio para definir as faixas.

```powershell
& C:/Users/gabri/.platformio/penv/Scripts/python.exe tools/maps/analyze_addresses.py
& C:/Users/gabri/.platformio/penv/Scripts/python.exe tools/maps/analyze_addresses.py --repack
```

O primeiro comando usa o ZIP já baixado e processa toda a cidade em streaming;
o segundo reaproveita os binários para medir compressão e faixas menores.
