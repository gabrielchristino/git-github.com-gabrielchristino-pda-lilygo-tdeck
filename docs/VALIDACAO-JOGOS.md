# Validação dos jogos — 19/10/2026

Alvo: T-Deck original, COM4. Upload e testes autorizados pelo usuário.

## Firmware instalado

- `platformio run -e tdeck`: SUCCESS.
- RAM estática: 24.332 bytes / 327.680; aplicativo: 748.897 bytes / 6.553.600.
- Imagem: 749.264 bytes; gravação somente do app em 0x10000, com hash verificado.
- SHA-256: `DB982E92B52232FCE90D451AD9BD48B48AA169C7DCE3D5DC0FCD053B6DC678E4`.
- Mesma base PlatformIO/Arduino/LVGL da revisão anterior; sem dependências novas.

## Testes no computador

`tests/run.ps1` passou com `-Wall -Wextra -Werror`, incluindo a suíte anterior.

- 2048: fusões sem fusão dupla, pontuação, sorteio, movimento sem efeito,
  desfazer com restauração do PRNG e continuação depois de atingir 2048.
- Sokoban: as 12 soluções geradas são executadas pelas regras reais, validando
  cada estado; desfazer e progressão para a próxima fase.
- Campo Minado: primeira abertura segura em todas as 81 posições, dez minas,
  contagem de vizinhos, flood-fill, bandeiras, vitória e derrota.
- Sudoku: os 16 puzzles têm solução única por solver independente; pistas fixas,
  entrada/desfazer, dicas, solução completa, ausência de conflitos e próximo puzzle.
- Persistência: fixture do payload antigo/schema 1 migra sem perder nota UTF-8;
  schema 2 preserva os quatro jogos, undo e sessão. Estados inválidos/schema
  futuro são rejeitados.

O renderer nativo usa os mesmos apps e LVGL do aparelho. Passou na navegação por
teclado dos quatro jogos, cancelamento de nova partida, ajuda, bloqueio de abertura
de casa com bandeira, primeira abertura segura, números por Enter no Sudoku,
digitação direta, dicas/desfazer e recriação da tela real do jogo após reload.
Capturas em `docs/previews/game-*.png` e `games-menu.png` foram conferidas, incluindo
ajuste de altura para os tabuleiros caberem inteiros no corpo da tela.

`tools/generate_game_levels.py` produz os dados próprios de forma determinística
e verifica os Sokobans por simulação reversível e os Sudokus por contagem de soluções.

## Testes no aparelho

- Antes da gravação: STORAGE=1, geração 136, uma nota e um compromisso, sem outras
  coleções pessoais. `PDA SAVE` confirmou o checkpoint.
- Depois da atualização: STORAGE=1, geração 137, mesmas contagens; os quatro jogos
  começaram vazios. Migração real do estado antigo ocorreu sem formatar a flash.
- Por comandos USB de tecla: abertura dos quatro jogos, quatro movimentos no
  2048, sete passos de Sokoban, abertura segura no Campo Minado e edição/dica no
  Sudoku. A interface respondeu sem reiniciar inesperadamente.
- Após `PDA SAVE` + `PDA RESTART`, o boot retomou Jogos/Sudoku, vista 8, geração 149.
  Os quatro registros de `PDA GAMES` coincidiram antes e depois: CRC dos tabuleiros
  e marcações, seleção, fase, pontuação, recorde, movimentos e situação da partida.
- Ao final, partidas reiniciadas pelas confirmações normais e menu Jogos aberto:
  STORAGE=1, DIRTY=0, geração 163, nota e compromisso preservados. O recorde mínimo
  de 4 pontos do ensaio de 2048 permaneceu; Sudoku ficou no puzzle 2/16.

`PDA GAMES` / `tools/device.py games` exibe metadados e CRCs das partidas, sem
conteúdo pessoal. Não existe visão remota da tela física: o layout foi verificado
nas capturas nativas. Digitação manual, conforto e legibilidade na tela do aparelho
ainda dependem do uso do usuário. Não houve teste novo de corte de bateria.

## Expansão da Masmorra

- 150 andares gerados em 50 sementes: conectividade, caminho até as três saídas,
  limites fechados e estado válido a cada passo.
- Combate: ogro em dois golpes e contra-ataque lento, defesa, espera, cura com
  limite de 20, duração de seis turnos do escudo e ações recusadas sem gastar turno.
- Inventário: coleta de cura/escudo/bomba, consumo, explosão por distância de
  caminho e bloqueio por parede, rejeição de inventário e duração inválidos.
- Snapshot: retomada de grade antiga 11×9 e nova 19×13 com inventário e escudo;
  os demais jogos continuam no mesmo formato. O campo antes sem uso `undoSeed`
  do save antigo é normalizado de 1 para 0 ao carregar.
- Preview LVGL: teclas Q/F/X, ajuda, retorno ao menu com layout normal e retomada
  em tela cheia. Capturas `game-dungeon`, `game-dungeon-map` e `game-dungeon-help`.
- Fuzz de ações inclui as novas teclas; nenhuma ação pode deixar save inválido.

A validação por renderer e compilação não substitui teste de legibilidade e
resposta no T-Deck físico; esta alteração não grava firmware no aparelho.

## Todos os jogos em tela cheia — 02/11/2026

- Forca: banco ampliado de 128 para 384 palavras, em 24 temas; os 128 índices
  originais foram preservados. Vitória, derrota, acentos, letras repetidas e
  unicidade são verificados para todas as palavras. Alfabeto com cores e indicador
  de seis tentativas na interface.
- Caça-palavras: 480 grades solucionáveis (20 sementes × 24 temas), com as seis
  palavras verificadas e selecionadas nas duas direções. Novos IDs usam quatro
  dígitos hexadecimais; saves antigos com dois dígitos continuam válidos e podem
  ser jogados até a vitória. Roundtrip de snapshots cobre os dois formatos;
  IDs fora do banco são rejeitados. O schema do snapshot permanece 4.
- Renderer LVGL: todos os dez jogos verificados em 320×240, cabeçalho oculto,
  conteúdo dentro da tela e acima do rodapé, sem rolagem vertical. Controles de
  ajuda, nova partida, troca entre jogos e retorno ao menu continuam cobertos.
- Telas maiores: 2048 com peças e números ampliados, Sokoban 22 px por casa,
  Sudoku/Minado 20 px, Naval 19 px, Cobrinha 19×20 px e Caça-palavras 19×20 px.
  Senha Secreta mostra as dez tentativas em duas colunas. Regras e partidas
  mantidas; nenhuma dependência nova.
- `tests/run.ps1`, testes ampliados de jogos, preview nativo e build PlatformIO:
  passaram. Aplicativo: 6.368.297 bytes / 6.553.600 (97,2%); RAM estática:
  27.964 bytes / 327.680. Capturas atualizadas em `docs/previews/game-*.png`.
- Esta revisão foi compilada e renderizada no computador; ainda não foi gravada
  nem avaliada no T-Deck físico.

## Grades sem painéis — 02/11/2026

- Caça-palavras mantém 10×8 casas, com células de 31×26 px, sem título nem aviso
  de Enter. A lista de seis palavras, tema e progresso ficam em Ajuda → Palavras;
  a aba Controles contém as instruções.
- Campo Minado mantém 9×9 e 10 minas; células de 35×23 px ocupam toda a largura.
  Contagens foram movidas para Ajuda → Partida. 2048 mantém o padrão 4×4, agora
  com células de 79×53 px; pontos/recorde/jogadas ficam em Ajuda → Partida.
- Os três jogos ocultam o rodapé e preservam os botões compactos no topo. Regras,
  posições, progresso e formato de salvamento permanecem iguais.
- Preview: limites de tela sem rolagem, abas Palavras/Controles/Partida, retorno
  para a grade e preservação da seleção do Caça-palavras conferidos. Capturas
  novas: `game-wordsearch-words`, `game-wordsearch-help`, `game-mines-stats` e
  `game-2048-stats`. Build nativo e firmware compilados; ainda sem gravação.

## Mais casas com fonte menor — 02/11/2026

- Novas partidas dos dois jogos usam 16×13 (208 casas), células de 19×16 px e
  fonte Montserrat 12 já incluída no firmware. Sem rolagem e com lista na ajuda.
- Campo Minado: 40 minas (168 casas seguras), densidade maior que as dez minas
  em 81 casas da versão anterior. Primeira abertura e vizinhança protegidas.
  Contagens conferidas por cálculo independente em todas as 208 aberturas
  iniciais; vitória, derrota, limites e snapshot verificados no tamanho novo e
  no antigo. Fixtures dos schemas 2/3 conservam os tabuleiros antigos reais.
- Caça-palavras: dez palavras por rodada, até 13 letras, máscara de progresso
  com dez bits. 480 grades completas resolvidas por busca independente; dois
  formatos de IDs antigos (dois/quatro dígitos) e seis palavras ainda carregam
  e permitem concluir a rodada. Seleção diagonal, invertida, limites, IDs
  inválidos e snapshot também cobertos.
- Preview verifica toque na casa 207, preservação da seleção ao abrir as abas
  de ajuda e ajuste das dez palavras na lista. As capturas foram conferidas;
  o marcador de casa fechada usa ponto ASCII, presente na fonte de 12 px.
- Suíte nativa e renderer passaram. Sem mudanças no schema 4; as dimensões são
  derivadas do tamanho da grade salva. Esta revisão ainda não foi instalada.

Correção visual posterior: casas fechadas do Campo Minado passaram a ser
retângulos preenchidos com borda inferior/direita, sem qualquer caractere.
Casas abertas usam o fundo claro; números e bandeiras continuam em ASCII.
Preview conferido e testes de interface passaram nos tabuleiros em tela cheia.

Auditoria completa de glifos: [cobertura, correções e capturas](AUDITORIA-CARACTERES.md).
O preview agora rejeita placeholders e legendas de peças/casas cortadas.

## Sokoban ampliado — 02/11/2026

- Painel lateral e rodapé removidos da partida. Tabuleiro ocupa a área abaixo
  dos botões; estatísticas, legenda e instruções estão na Ajuda.
- 24 fases: as 12 originais/índices de 8×8 preservados e 12 novas de 16×12,
  com quatro a seis caixas e divisórias. O gerador confere piso conectado,
  embaralhamento por puxadas reversíveis e solução por simulação independente.
- Todas as 24 soluções também executadas pelas regras C++ reais, verificando
  estado a cada passo, desfazer e avanço entre fases. Snapshot preserva grade,
  posição e desfazer em todas as fases. Nenhuma mudança no schema 4.
- Ajuda com Partida/Controles/Fases; seleção de uma das 24 fases pede confirmação
  antes de substituir a rodada atual. Cancelar preserva grade e passos.
- Preview cobre seleção/cancelamento/confirmação da fase 13, movimento/desfazer
  na grade maior, limites sem rolagem e abas. Capturas `game-sokoban-large`,
  `game-sokoban-phases`, `game-sokoban-stats` e `game-sokoban-help` conferidas.
- Auditoria de fontes: 256 combinações, 25.540 verificações e 4.881 labels; sem
  glifos ausentes ou cortados nas casas/peças. Testes nativos passaram.
- Firmware compilado para o T-Deck; esta revisão ainda não foi instalada.

## Todas as fases do Sokoban em 16×12 — 02/11/2026

- As fases 1–12 foram substituídas por novos tabuleiros de 16×12, com quatro a
  seis caixas; as fases 13–24 maiores mantiveram seus layouts e índices.
- As 12 grades antigas de 8×8 permanecem somente para validar e continuar saves
  existentes, inclusive desfazer. Nova/troca/avanço passa para a grade maior.
  O formato de snapshot continua 4, sem reinício automático de partidas.
- Gerador prova conectividade e solução das 24 grades grandes. Testes C++
  resolvem as 24 grandes e as 12 antigas, com roundtrip de save, desfazer,
  progressão e fixtures de migração dos schemas 1/2/3.
- `tests/run.ps1` e preview passaram; auditoria: 254 combinações, 25.554
  verificações, 4.909 labels, sem glifos ausentes ou cortados nas casas/peças.
- Capturas regeneradas, incluindo a fase 1 maior em `game-sokoban.png`.
  Firmware compilado; esta revisão ainda não foi instalada.
