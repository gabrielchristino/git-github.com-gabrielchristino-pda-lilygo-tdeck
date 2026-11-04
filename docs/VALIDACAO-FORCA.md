# Forca em português — 19/10/2026

- Banco próprio de 128 palavras em oito temas, sem downloads/dependências novas.
- Todas as letras A–Z são tentativas, inclusive WASD, M e N. Enter volta aos jogos;
  Backspace pede nova palavra com confirmação; espaço após concluir inicia outra.
- Maiúsculas/minúsculas equivalentes; normalização de acentos e cedilha preserva
  a grafia original na tela. Repetir letra não altera tentativas/erros. Seis erros
  encerram a rodada; a palavra é revelada. Vitórias são acumuladas.

## Validação

- `tests/run.ps1` passou: regras anteriores e as 128 palavras da Forca, todas
  únicas e normalizáveis para A–Z; vitória/derrota, repetição, acentos, contagem
  de vitórias, entrada inválida e proteção após o término.
- Fixtures de schemas 1 e 2 passaram no leitor novo/schema 3. O caso schema 2
  inclui partidas iniciadas e verifica a preservação das quatro anteriores.
- Renderer nativo passou: menu com cinco jogos, todas as letras como tentativas,
  vitória com `camaleão`, derrota, cancelamento de nova palavra, sorteio sem
  repetição imediata, retorno ao menu e recriação da tela com tentativas salvas.
- Capturas conferidas: `game-forca.png`, `game-forca-win.png`, `game-forca-loss.png`.
  O desenho fica em contêiner próprio, sem sobrepor os controles superiores.
- Compilação ESP32-S3: SUCCESS. RAM estática 24.468 bytes; app 754.833 bytes;
  imagem 755.200 bytes.
- SHA-256: `2D4EBA3F6501EC1B85452F803237684C07E33817209762B03EA281822B327930`.

## Aparelho

- Antes: COM4, STORAGE=1, geração 165, uma nota e um compromisso. Checkpoint
  confirmado; registrados apenas os metadados/CRCs das quatro partidas existentes.
- Gravação apenas do app em 0x10000; hash verificado. Boot em geração 166 com
  documentos preservados e quatro registros de partida idênticos, após migração
  real do schema 2 para 3.
- Por teclas USB, abriu Forca e recebeu `wasdmn` como seis tentativas, com cinco
  erros; repetir `W` não mudou o estado. Enter/menu e reabertura mantiveram a rodada.
- Após salvar e executar ESP.restart, geração 174: CRCs da palavra/letras, erros,
  tentativas, fase, recorde e situação coincidiram nos cinco jogos. As quatro
  partidas antigas continuaram exatamente iguais aos metadados anteriores.
- Nova palavra pela confirmação normal; Forca deixada aberta sem tentativas:
  STORAGE=1, DIRTY=0, geração 176. Documentos pessoais e outros jogos preservados.
- Não houve inspeção remota da tela física nem teste novo de corte de bateria.
  As capturas são da interface real em renderer nativo; teclas físicas do jogo
  ainda dependem da experiência do usuário.
