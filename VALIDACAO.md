# Validação da versão 0.2

Data: 19 de outubro de 2026. Alvo: T-Deck original. Instalação e testes no aparelho
foram autorizados pelo usuário. Histórico: [base 0.1](docs/VALIDACAO-0.1.md).

A revisão mais recente acrescenta quatro jogos persistentes e migração do schema
1 para 2. Build, hash, testes de regras/interface e retomada após reinício físico:
[Validação dos jogos](docs/VALIDACAO-JOGOS.md).

A revisão seguinte acrescenta **Forca em português**, 128 palavras, quinto estado
de partida e schema 3 com leitura dos anteriores:
[Validação da Forca](docs/VALIDACAO-FORCA.md).

Uma revisão posterior adicionou contadores brutos para investigar a direção
esquerda do trackball. O firmware e os resultados dessa revisão estão em
[TRACKBALL.md](docs/TRACKBALL.md); o hash abaixo identifica a revisão 0.2 anterior.

## Revisão de navegação pelo teclado (19/10/2026)

### Ampliação para todos os apps

- WASD agora navega no launcher, listas, botões, confirmações e paleta de símbolos,
  além dos conteúdos espaciais. Shift+$ alterna busca/texto com navegação; nos
  campos, move o cursor sem inserir letras. Enter sai do modo de cursor para os
  botões; campos abertos/restaurados começam em modo texto.
- Testes nativos passaram: launcher nos dois eixos, busca por nomes iniciados
  por WASD, criação/edição/exclusão de contato apenas por teclas, confirmação por
  A/Enter, botões de Ajustes, cursor de nota e escrita literal de `wasd$`.
- `tests/run.ps1` passou. Capturas do início, modo texto e modo de cursor conferidas.
- Build: SUCCESS; RAM estática 23.788 bytes, app 730.845 bytes, imagem 731.216 bytes.
- SHA-256: `5E23D402C96C087D213200F6D92B2A4C7D120A41AEFD3FFCBBFA2FDBC57B8F35`.
- Instalado na COM4 somente em 0x10000; hash verificado. Estado salvo antes da
  gravação. Boot com STORAGE=1, uma nota e nenhuma planilha, iguais ao estado
  observado imediatamente antes desta atualização.
- No aparelho, comandos USB de tecla passaram por início, busca com byte 0x04,
  Ajustes, calendário e lista de notas. Nenhum conteúdo ou preferência foi editado.
  Ao final, início aberto, DIRTY=0, geração 132, contagens de documentos preservadas.
- A combinação física Shift+$ vem do código do teclado original (0x04, como na
  referência local `t-deck-os/src/main.cpp`); sua digitação física ainda não foi
  confirmada pelo usuário. O caminho USB passa pela mesma ação da interface.

### Primeira revisão (conteúdo de três apps)

- W/A/S/D move o conteúdo de calendário, desenho e planilha; letras minúsculas
  e maiúsculas funcionam. Campos de edição preservam essas letras como texto.
- A grade atravessa páginas até Z100 e retorna a A1 sem eventos de bola/touch;
  limites, retorno ao conteúdo após menu e escrita de `wasd WASD m` passaram no
  renderer nativo. Calendário e desenho também passaram nos quatro sentidos.
- Digitar no menu da grade não inicia substituição do conteúdo de uma célula.
- Layout do rodapé conferido na captura nativa de planilha.
- Compilação: SUCCESS; RAM estática 23.788 bytes, aplicativo 730.409 bytes,
  imagem 730.768 bytes.
- SHA-256: `785779EACCE6908CA408A7E38A0762254335B5988B6B64F831660E88BD9B35AF`.
- Estado salvo por `PDA SAVE` antes da gravação. Aplicativo instalado no aparelho
  pela COM4, com hash verificado pelo esptool; partições de dados preservadas.
- A navegação por teclas físicas ainda depende de confirmação do usuário; os
  testes de navegação acima usam o código real da interface no computador.

## Firmware

`platformio run -e tdeck`: **SUCCESS**.

| Componente | Versão |
| --- | --- |
| espressif32 | 6.3.0 |
| Arduino-ESP32 | 2.0.9 |
| LVGL | 8.4.0 |
| TFT_eSPI | 2.5.43 |
| SensorLib | 0.2.0 |

- RAM estática do linker: 23.684 bytes de 327.680 (7,2%).
- Aplicativo: 729.481 bytes de 6.553.600 (11,1%).
- Imagem `firmware.bin`: 729.840 bytes.
- Stack de loopTask: 32 KiB, para o parser recursivo com limites e serialização.
- A RAM estática não inclui essa stack, objetos LVGL, documentos ou buffers dinâmicos.

SHA-256 de `.pio/build/tdeck/firmware.bin`:

```text
9FFFD3B879A134358842EC3AF37BA214DFF80DBCEB09A07A62C98A115D2EFB17
```

## Testes no computador

`tests/run.ps1`: **passou**, com `-Wall -Wextra -Werror` nos testes portáveis.

- Debounce, clique curto/longo, rollover e loop atrasado.
- Precedência, sinais, decimais, funções, intervalos, texto, vazios, limites,
  referências inválidas, erros numéricos e divisão por zero.
- Ciclos diretos/indiretos, limites de profundidade, edição de dependências e
  capacidade da grade esparsa.
- Roundtrip de documentos, UTF-8, desenho, fórmulas, sessão e rascunhos.
- CRC incluindo metadados de geração; corrupção e truncamento.
- Interrupção ao gravar a cópia inativa, recuperação da cópia válida, gravação
  seguinte e proteção contra sobrescrita quando ambas estão inválidas.
- Datas inválidas/bissextos, fuso, temporizador, cronômetro, lembretes, pausa após
  perda da hora e autosave durante rollover.

O renderer nativo compila **o mesmo código dos apps e do shell LVGL**. Passou em:

- Criação e edição de notas, contatos, agenda, desenhos e planilhas.
- Inserção de `ç` pela paleta e composição de `_-` para `=`.
- Fluxo completo de criar/editar/excluir contato somente por eventos de teclado
  e trackball, incluindo seleção de vários campos e confirmação de exclusão.
- Navegação de mês/dia e retorno ao mês sem touch.
- Desenho pela bola e cancelamento da limpeza preservando os pixels.
- Entrada de `=A1*2` na planilha, com resultado 24 para A1=12.
- Calculadora, início de timer, salvamento e restauração de documentos.
- Recriação do shell após carregar o snapshot: app Notas, documento, modo de edição,
  cursor e conteúdo restaurados.

As capturas de início, notas, contatos, agenda, calendário, desenho, planilha,
calculadora, relógio, ajustes e editor restaurado foram inspecionadas. Corrigidos
contraste e quebra indevida do botão Nome. Os dados das capturas são fixtures,
não cópias dos dados pessoais do aparelho.

Na revisão final, rodapé e relógio passaram a atualizar os objetos de texto
somente quando o conteúdo muda, evitando redesenhos e alocações contínuas em
repouso. A suíte de interface passou novamente após esse ajuste.

## Testes no T-Deck físico

A versão 0.2 foi gravada na **COM4**, somente em `0x10000`, com esptool 4.5.1.
A ferramenta confirmou o hash após a gravação. Bootloader, tabela de partições e
região de dados ficaram fora dos intervalos apagados pela atualização.

O firmware respondeu pela USB e confirmou LittleFS ativo, sem alterações
pendentes e com uma nota no estado. Foram enviados eventos de teclado e bola
pelo protocolo local para exercitar os mesmos handlers usados pelo hardware.

Depois de `PDA SAVE`, foi feito **reinício real por ESP.restart()**:

```text
Antes: READY=1 STORAGE=1 GEN=19 DIRTY=0 APP=9 HOME=1 NOTES=1
Depois: READY=1 STORAGE=1 GEN=20 DIRTY=0 APP=9 HOME=1 NOTES=1
```

O aumento para geração 20 veio do checkpoint de `PDA RESTART` antes do reset.
O log mostrou boot normal, montagem da flash e recuperação do estado. As
contagens de notas, contatos, eventos, planilhas e desenhos coincidiram antes e
depois. Não foram lidos nem registrados os textos pessoais da nota.

A otimização final foi instalada após outro checkpoint. Após a atualização
somente do aplicativo, a placa confirmou `STORAGE=1 GEN=21 DIRTY=0 NOTES=1`, com
app e estado do início preservados. Isso verificou também a preservação dos
arquivos através de uma atualização de firmware real.

PSRAM reportada no boot: 8.386.279 bytes; livre nesse ponto, antes de reconstruir
a interface, 8.360.679 bytes. Isso não representa pico de uso de memória.

O usuário já havia confirmado fluidez do teclado/trackball na base 0.1. Não foi
feita observação visual remota da tela da 0.2; as capturas de layout são do renderer.

## Limites da validação

- Não foi desconectado o cabo, removida a bateria ou simulada queda física de tensão.
  Recuperação de escrita interrompida foi testada no backend de teste; recuperação
  após reset foi confirmada na flash real.
- Exportação para SD e saída audível do alerta não foram confirmadas manualmente.
- Não há medição de autonomia, indicador calibrado de bateria, deep sleep ou NTP.
- Não há garantia de alarme com alimentação desligada; hora desconhecida exige ajuste.
- Dependências de hardware ainda usam as pastas locais de referência.

A instalação de referência em `t-deck-os` permaneceu sem alterações.
