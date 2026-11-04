# Arquitetura do PDA 0.2

## Alvo e stack

T-Deck original, ESP32-S3, tela 320 × 240, teclado I²C `0x55`, trackball e GT911.
A flash de 16 MB foi identificada no aparelho; o boot confirmou cerca de 8 MB de
PSRAM. Firmware C++/Arduino sobre o runtime FreeRTOS do ESP32, PlatformIO e LVGL
8.4. As bibliotecas físicas e a definição da placa estão incluídas em
`lib/` e `boards/`, com as configurações e licenças da referência local preservadas.

Código próprio do shell e dos aplicativos fica em `pda-os`. Nenhum arquivo de
`t-deck-os/src` participa da compilação. Os projetos CircuitPython/Beryllium,
`tdeck_repl`, `tdeck-config` e `gt911touchutils` foram preservados como referência.

## Camadas implementadas

| Camada | Responsabilidade e arquivos |
| --- | --- |
| HAL | `src/hal/tdeck.cpp`: display, I²C, touch e contagem de pulsos |
| Serviços de placa | `src/hal/services.cpp`: LittleFS, SD, iluminação, hora e som |
| Modelo | `src/core/model.cpp`: notas, contatos, eventos, planilhas, bitmaps e sessão |
| Persistência | `src/core/storage.cpp`: codec versionado, CRC, duas cópias e recuperação |
| Runtime | `src/core/system.cpp`: autosave, hora confiável, cronômetro, timer e lembretes |
| Expressões | `src/core/expression.cpp`: parser limitado, referências e agregações |
| Shell | `src/ui/shell.cpp`: início, busca, troca de app e alertas globais |
| Workspace | `src/ui/workspace.cpp`, `forms.cpp`: foco, menus, edição, símbolos e confirmação |
| Aplicativos | `src/apps/organizer.cpp`, `tools.cpp`: telas dos apps pessoais |
| Jogos | `src/core/games.cpp`: regras e validação; `src/apps/games.cpp`: telas e controles |

Um app visual fica ativo por vez. Os serviços continuam rodando ao trocar de tela.
Eventos de UI e mutações do modelo ocorrem no loop; ISRs apenas contam pulsos.
Callbacks de botões enfileiram uma ação, aplicada fora da travessia de eventos do
LVGL. Alocações grandes podem usar PSRAM, com buffer de tela parcial de 25.600 bytes.

## Entrada

A bola preserva X e Y, com pulsos agregados a cada 45 ms. Listas usam navegação
por foco; calendário, planilha e desenho usam deslocamento espacial. `m` alterna
esses apps para os botões de menu. A tecla mantém seu significado de texto dentro
de um campo, inclusive na planilha após entrar na edição.

WASD é convertido em movimento no shell e no workspace conforme o contexto:
menus, conteúdo e confirmações navegam; campos e buscas em modo texto recebem
as letras. Shift+$ (byte 0x04 do teclado original) alterna digitação/busca com
navegação. Nos campos, navegação move o cursor e Enter encerra a edição. Na
paleta, W/S avança uma linha de oito símbolos. Os modos de teclado são locais à
interface e começam em texto ao abrir/restaurar um campo, sem mudar o snapshot.

O app Jogos usa a vista 8 e o campo de sessão `document` de 1 a 10 para indicar
o jogo aberto (0 é o menu). Os dez estados de partida são independentes dos
documentos pessoais. As regras são portáveis e testadas sem LVGL. Fases e puzzles
próprios são gerados no desenvolvimento, com verificação de solução; a placa não
precisa resolver ou gerar um Sudoku em tempo de execução. O schema 2 acrescenta
as quatro partidas iniciais e mantém leitura do schema 1. O schema 3 acrescenta
a Forca e mantém leitura de ambos. O banco de palavras é próprio e a normalização
de acentos usa a mesma função dos filtros de busca, preservando a grafia na tela.

`core/more_games.cpp` contém os cinco jogos adicionais; `apps/more_games.cpp`
contém suas telas. O schema 4 aceita os três anteriores e acrescenta os novos
estados. Apenas Cobrinha depende de tick: um passo a cada 350–126 ms conforme a
pontuação, com direção pendente limitada a uma curva por passo e pausa ao sair,
restaurar ou receber avisos. Batalha Naval usa caça aleatória e vizinhança dos
acertos conhecidos. Caça-palavras compartilha o banco próprio de português e
sempre posiciona seus seis alvos. Masmorras são labirintos conectados, com neblina
explorada persistente e inimigos que agem apenas depois do jogador.

Clique curto é emitido ao soltar. Segurar por 700 ms solicita início, sem clique
curto anterior. Textareas recebem caracteres, backspace e movimentos de cursor.
Clique encerra a edição para navegar por campos e ações. A paleta de símbolos e
a composição de pares evitam depender de modificadores que o teclado não exponha.
A fonte derivada de DejaVu inclui português e usa os ícones LVGL como fallback.

## Documentos e sessão

`State` armazena notas, contatos, eventos, quatro planilhas esparsas e oito bitmaps
monocromáticos. Agenda e calendário compartilham eventos. Calculadora e planilha
compartilham o parser aritmético. Fórmulas são preservadas como texto original.

A sessão inclui app e vista, estado do início, documento, campo, caret, célula,
dia, cursor do desenho, caneta e rascunhos. Notas, contatos e células recebem
alterações durante a digitação. Datas incompletas dos eventos ficam em rascunhos
duráveis por evento, sem substituir o horário válido até confirmar Salvar.

[Persistência e recuperação](PERSISTENCIA.md) descreve o formato e o protocolo de
gravação. Preferências fazem parte dos snapshots; esta versão não usa NVS para
armazenar documentos ou preferências. A partição existente permanece inalterada.

## Hora e energia

O serviço consulta a hora do ESP32 e aceita ajuste manual, com fuso fixo em minutos
(padrão UTC−3). `lastKnownUtc` é referência persistida, nunca tratada como hora
atual após perda de alimentação. Lembretes dependem de hora confiável e aparelho
ligado; cronômetro usa deltas monotônicos e restaura pausado.

O timer persiste restante e prazo UTC. Retoma pela hora quando confiável ou fica
pausado com o restante do checkpoint. Alarmes são globais e o estado de disparo
é salvo. O som tem duração limitada. A tela reduz e apaga por inatividade, mas o
loop continua executando. A primeira interação acorda sem acionar um controle.

## Fórmulas

Parser sem avaliação de código: `+ - * /`, parênteses, sinais, decimais, referências
A1:Z100, intervalos e SUM/AVG/MIN/MAX/COUNT. Até 256 bytes por fórmula, 32 níveis
de dependência, limite sintático e orçamento de 8192 operações. O loopTask tem
32 KiB de stack para as chamadas recursivas limitadas.

Referências em avaliação detectam ciclos. Texto em aritmética gera erro; vazios
valem zero. Agregações ignoram texto e vazios; SUM/COUNT sem números retornam zero,
AVG/MIN/MAX sem números retornam erro. Erros de células se propagam. O orçamento
interrompe casos excessivos. O recálculo lê os conteúdos atuais, sem cache durável
que possa ficar obsoleto após uma edição.

## Exportação e atualizações

O SD é opcional. Cada exportação cria um diretório novo com snapshot completo,
TXT, CSV e PBM. CSV contém valores; o snapshot conserva fórmulas e rascunhos.
Atualizações pelo script gravam apenas o app, mantendo partições e arquivos.

## Extensões futuras

NTP/sincronização, importação interativa de backups externos, recorrência de
agenda, deep sleep com despertador validado, bateria calibrada e pacote de
bibliotecas independente das pastas de referência. A versão 0.2 implementa o
primeiro escopo funcional de todos os apps; essas extensões não estão anunciadas
como prontas.

## Referências

- [LilyGO T-Deck](https://github.com/Xinyuan-LilyGO/T-Deck).
- [LVGL e entradas](https://docs.lvgl.io/8.3/overview/indev.html).
- [littlefs e recuperação após falta de energia](https://github.com/littlefs-project/littlefs).
- [Fonte DejaVu e licença local](../licenses/DejaVu.txt).
- [Registro de validação](../VALIDACAO.md).
