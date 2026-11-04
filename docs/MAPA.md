# Mapa vetorial: Marginal Pinheiros / Zona Sul

O aplicativo **Mapa** desenha linhas e nomes diretamente com LVGL. Não usa PNG,
JPEG, tiles raster nem imagens salvas no microSD. Os vetores ficam na flash do
aplicativo e são lidos diretamente, sem carregar o mapa inteiro em RAM.
Funciona offline e sem cartão SD. O framebuffer normal da tela é transitório.
O mapa ocupa **320 × 240 pixels**, com cabeçalho e rodapé gerais recolhidos.
Uma barra compacta mostra estado do GPS, norte e o botão Menu. A escala fica
no canto inferior e os controles aparecem somente quando solicitados.
O desenho reutiliza um cache transitório de linhas já projetadas para a vista
atual e uma margem de 96 pixels. Um índice espacial seleciona somente os
trechos próximos; pequenos movimentos reutilizam essas coordenadas. O recorte
usa aritmética inteira e não precisa ser repetido por faixa do framebuffer.
Durante o movimento, aparecem rio e vias maiores. Ruas menores, ciclovias,
ferrovias e nomes voltam após 350 ms sem movimento. A bolinha desloca 4 pixels
por pulso e WASD mantém 16 pixels. Os nomes podem ser ocultados com L.

## Área e dados

Recorte de São Paulo entre latitudes -23.755 e -23.520 e longitudes -46.810 e
-46.590, cobrindo o corredor da Marginal Pinheiros e entorno, incluindo Morumbi,
Santo Amaro e a região de Interlagos. Não é toda a cidade nem toda a Zona Sul.
Vista inicial na região da Marginal próxima ao Morumbi / Chácara Santo Antônio.

Extração OpenStreetMap em 28/10/2026: **62.283 trechos**, **206.351 pontos** e
21.478 nomes distintos. Inclui ruas residenciais, avenidas, vias principais,
ligações, ciclovias, rios/canais e ferrovias. Não inclui todos os caminhos de
serviço, edifícios, trânsito ou cálculo de rotas. Há uma base separada de
interpolação de números do IBGE, descrita abaixo.
Linhas foram recortadas na área e simplificadas com tolerância de 2 metros.
Não é um mapa de navegação veicular com instruções de conversão.

A projeção local usa metros aproximados, com norte para cima, e nove níveis de
zoom de 1 a 256 metros por pixel. A escala gráfica acompanha o zoom. Menos ruas
e nomes são desenhados nos níveis mais distantes para manter a legibilidade.

## Controles

- **W/A/S/D ou trackball:** deslocar a vista.
- **I / O** (ou + / -): aproximar / afastar.
- **Clique da bola / Enter:** aproximar no modo de exploração.
- **F:** centralizar e seguir o GPS, se houver posição válida dentro do recorte.
- **C:** retornar à vista inicial.
- **M ou botão Menu:** abrir/fechar controles. O painel inclui zoom, GPS,
  Centro, Nomes, Ajuda, Início e Fechar. WASD/Enter navegam no painel aberto.
- **L:** mostrar/ocultar nomes, para uma vista mais limpa das linhas.
- **H ou ?:** informações de licença e limitações.
- **B ou Menu > Buscar:** pesquisar rua ou `rua, numero` de forma aproximada.
- **Touch:** arrastar o mapa e abrir Menu para acessar os controles.
- **Segurar a bola:** início, como nos outros aplicativos.
- **Menu > Início:** voltar ao launcher pelo touch. Backspace fecha o painel.

Centro e zoom são salvos junto da sessão existente do PDA. O modo de seguir GPS
é transitório. Nenhum documento pessoal é apagado pela atualização do aplicativo.

## Busca de vias

Digite parte do nome e pressione Enter ou toque em Pesquisar. Maiúsculas e
acentos são ignorados: `nacoes unidas` encontra `Nações Unidas`. Prefixos
Rua/Avenida/Ponte/Estrada também aceitam as abreviações usadas no mapa.
Na comparação, de/do/da/dos/das são ignorados como palavras inteiras, para
aceitar variações entre o cadastro de numeração e o mapa.
São exibidos até cinco nomes distintos, priorizando início de palavra e
proximidade da vista atual. Use a bolinha para escolher e Enter para abrir,
ou toque no resultado. O mapa centraliza em um ponto do trecho encontrado e
aproxima para 4 metros/pixel. Voltar retorna ao mapa; Backspace apaga texto.

A busca sem vírgula usa os nomes dos vetores já gravados, sem internet, GPS ou
arquivos de imagem. Não pesquisa CEPs ou bairros. Um resultado de rua representa
um trecho da via. A consulta é transitória e não altera a sessão salva até
escolher um resultado.

Com vírgula, por exemplo `nacoes unidas, 12501`, pesquisa **número aproximado**.
Usa pontos de apoio do IBGE CNEFE 2022 e interpola entre números do mesmo lado
par/ímpar e faixa observada, sem extrapolar. Até três alternativas aparecem
com CEP. Ao escolher, um X laranja e o texto `Endereco aproximado` identificam
a estimativa; o zoom vai a 8 metros/pixel. Esse X não é o GPS azul.
Não comprova a existência de um imóvel nem sua entrada exata. Números sem
faixa, complementos e números com letras recebem ausência de estimativa.
C ou uma busca só de rua removem o alvo; F o remove quando segue um GPS válido.
O alvo é transitório; centro e zoom continuam salvos como antes.

A base aproximada ocupa 2,67 MB e cobre as faixas disponíveis no município de
São Paulo dentro do recorte inteiro. Veja [método e limites](ENDERECOS-APROXIMADOS.md).
A [análise da base integral](ANALISE-ENDERECOS.md) registra a alternativa maior,
que continua fora do firmware.

## GPS

`Explorar | sem GPS` permite examinar o mapa manualmente. A cruz cinza marca
somente o centro da vista. **Ela não representa a localização do aparelho.**
O marcador azul só aparece com posição GNSS válida e recente. A tecla F ativa
o acompanhamento; mover o mapa volta ao modo de exploração. Perda de posição
retira o marcador, e uma posição fora da área recebe aviso sem ser limitada
artificialmente à borda do mapa. Não existem posições GPS simuladas no firmware.

O teste da interface no computador injeta uma posição apenas durante os testes;
capturas `map-test-*` são evidência de teste, não leituras do aparelho.

## Reproduzir os dados

```powershell
& C:/Users/gabri/.platformio/penv/Scripts/python.exe tools/maps/build_pinheiros.py --download
```

Isso consulta uma única região no Overpass e guarda JSON de origem em
`build/maps/pinheiros-osm.json`. Para repetir apenas a conversão offline:

```powershell
& C:/Users/gabri/.platformio/penv/Scripts/python.exe tools/maps/build_pinheiros.py
```

Saídas: `src/core/map_data.inc` e `data/maps/pinheiros.json`. O segundo registra
limites, timestamp OSM, consulta, tolerância e SHA-256 da fonte. A conversão não
produz imagens. Depois, compilar e gravar somente o aplicativo com o fluxo normal.

`tools/device.py map` consulta vetores, GPS, centro/zoom, candidatos do índice,
segmentos em cache e duração da última reconstrução de geometria pelo USB.

## Validação

- Testes de todos os trechos, índices, limites, projeção, recorte de segmentos,
  níveis de zoom, movimento, GPS ausente, GPS fora do recorte e perda de fix.
- Preview LVGL com navegação por teclado, botões, zoom, atribuição, posição
  injetada apenas em teste e restauração do centro/zoom pela persistência.
- Firmware compilado dentro dos 6,4 MB da partição do aplicativo.
- Instalado no T-Deck pela COM4: zoom e deslocamento por comandos de teclado
  confirmados, centro/zoom restaurados após reinicialização e nota/compromisso
  existentes preservados. Vista final deixada no aplicativo Mapa.
- Tela inteira validada no preview (320 × 240, barras gerais ocultas), com menu
  aberto/fechado, nomes opcionais e retorno ao launcher restaurando o layout.
  No aparelho, movimento, zoom, nomes, menu, Início e reabertura passaram; a
  maior resposta USB no roteiro foi de 0,547 s após o cache vetorial, contra
  cerca de 3 s na primeira versão de tela inteira sem essa otimização.
- Otimização de fluidez validada no preview: movimentos curtos reutilizam
  geometria, cruzar a margem reconstrói o cache e detalhes retornam ao parar.
  No T-Deck, reconstrução de geometria em 78–96 ms no roteiro, contra cerca
  de 500 ms antes do recorte inteiro. Respostas USB de 16 movimentos de teclado:
  125–219 ms. Em outro roteiro, 24 movimentos da bolinha: mediana de 63 ms e
  máximo de 94 ms. Essas medidas incluem resposta do comando, não são FPS.
- Busca validada com acentos/maiúsculas, nomes parciais, prefixos, deduplicação,
  limite de resultados, proximidade, consulta vazia e ausência de resultados.
  Preview cobre abertura por B/Menu, edição com Backspace, Enter, toque e
  preservação da vista quando não há resultado.
  No T-Deck pela COM4, a consulta `nacoes unidas` respondeu em aproximadamente
  1,4 s; seleção centralizou e aproximou a via. Consulta sem resultado,
  cancelamento e preservação da nota e compromisso existentes foram confirmados.
- Aplicativo com aproximadamente 3,6 MB. A gravação em 0x10000 teve hash verificado;
  bootloader, partições e LittleFS não foram regravados. GPS ainda sem posição
  no teste físico; acompanhamento foi validado somente por injeção no preview.

Dados: **© OpenStreetMap contributors**, ODbL 1.0.
<https://www.openstreetmap.org/copyright>
<https://opendatacommons.org/licenses/odbl/1-0/>
