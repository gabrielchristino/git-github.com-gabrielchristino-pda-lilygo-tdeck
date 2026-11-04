# Auditoria dos caracteres dos jogos — 02/11/2026

Resultado do renderer nativo, com as mesmas fontes e LVGL do firmware:
**254 combinações de fonte/caractere, 25.554 verificações e 4.909 labels**.
Nenhum glifo ausente ou cortado nas legendas das peças/casas testadas após as
correções. Compilação do firmware concluída; revisão ainda não instalada.

## Cobertura

- Textos literais dos apps e regras dos dez jogos, extraídos automaticamente
  dos quatro arquivos `games.cpp` e `more_games.cpp` de apps/core. O arquivo
  gerado acompanha as dependências do build e é recriado quando os textos mudam.
- Todas as 384 palavras e respectivos temas; acentos portugueses em maiúsculas
  e minúsculas, ASCII imprimível, reticências, ponto central e multiplicação
  na fonte portuguesa com seu fallback real.
- Fontes Montserrat 12, 16 e 28 nos alfabetos efetivamente utilizados: letras
  do Caça-palavras, números/bandeiras/minas, personagens/itens da Masmorra,
  vida/inventário e números do 2048/Senha Secreta.
- Labels visíveis verificadas durante os fluxos do preview, incluindo ajuda,
  seleção, retorno, telas de resultado existentes e restauração. Cada glifo
  precisa de descritor real e bitmap; placeholders são rejeitados.
- Nas peças/casas, os limites do bitmap são comparados à área interna do
  objeto, incluindo offsets, descendentes e bordas. O 2048 também foi
  renderizado com 2048, 8192, 16384 e 1073741824, cobrindo suas três fontes.

## Problemas encontrados e corrigidos

- O ponto central `·` (U+00B7) não existe na Montserrat 12 incluída. As casas
  fechadas do Campo Minado agora são desenhadas com preenchimento e borda,
  sem caractere. O símbolo continua válido na fonte portuguesa da Batalha Naval.
- O `@` da Masmorra tinha um pixel inferior fora da casa de 13 px: a fonte
  Montserrat 12 possui altura de linha de 15 px. A legenda foi deslocada um
  pixel para cima. O mesmo ajuste foi conferido para todos os símbolos desse jogo.

## Evidências

- [Relatório por fonte e código Unicode](game-glyph-audit.tsv)
- [Fonte portuguesa e acentos](previews/font-pda_font.png)
- [Fonte pequena](previews/font-Montserrat12.png)
- [Símbolos no tamanho real das casas](previews/font-game-symbols.png)
- [Números grandes do 2048](previews/game-2048-large-values.png)

Para repetir, compile `tools/preview` e execute `pda_preview.exe` no diretório
do build. A auditoria roda antes e durante os fluxos, produz o TSV e as
capturas `font-*.ppm`, e encerra com erro se encontrar um glifo ausente ou
uma legenda de peça/casa cortada. O exportador existente converte PPM para PNG.
O build do preview requer Python 3, já usado pelas ferramentas do projeto.

A inspeção física da legibilidade no T-Deck continua distinta da verificação
dos bitmaps e da renderização aqui descrita.

Atualização do Sokoban: fontes e alinhamento também conferidos nas grades 16×12 de todas as fases, nas três abas de ajuda e na confirmação de troca de
fase. [Captura da grade maior](previews/game-sokoban-large.png).
