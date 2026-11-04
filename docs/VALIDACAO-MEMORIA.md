# Validação da otimização no T-Deck

Data: 03/11/2026. T-Deck original, ESP32-S3, COM4. Instalação e testes por
comandos USB autorizados pelo usuário. Imagem de 5.205.328 bytes, aplicativo
de 5.204.969 bytes (79,4% da partição), RAM estática de 37.648 bytes.
SHA-256: `28a89d41e4b28ebcb683a6c967aa397025a15ade726af6b538d6936012f520bf`.

## Instalação e dados

Antes da gravação, `PDA SAVE` confirmou salvamento. Foram registrados estado,
contagens de documentos e os dez relatórios completos `GAME`, incluindo CRCs
dos tabuleiros, marcas e undo, pontuação, movimentos, sementes e demais campos.

`tools/flash.ps1 -Port COM4 -SkipBuild` gravou apenas o aplicativo em 0x10000.
O esptool verificou o hash dos dados e reiniciou o aparelho. Bootloader,
partições e área de documentos não foram regravados.

O teste depois da instalação confirmou `READY=1`, `STORAGE=1` e as mesmas
contagens: 1 nota, 1 contato, 3 compromissos, 0 planilhas e 0 desenhos. Os dez
relatórios de jogos coincidiram integralmente com os anteriores à atualização.
Não foi feita leitura ou comparação byte a byte do conteúdo de notas,
contatos e compromissos: a verificação desses documentos foi por contagem e
carregamento normal do armazenamento.

## Comandos e resultados

A validação por USB enviou comandos `PDA STATUS`, `SAVE`,
`GAMES`, `GPS`, `BALL`, `MAP`, `KEY`, `MOVE`, `HOME` e `RESTART`.

Passaram no aparelho:

- Navegação ao início e abertura dos aplicativos Agenda, Calendário, Notas,
  Desenho, Relógio, Contatos, Calculadora, Planilha, Jogos, Ajustes e Diagnóstico.
  Foram abertos apenas os menus/listas; nenhum documento ou jogo foi editado.
- Mapa com movimento, zoom nos dois sentidos, ajuda, retorno e centro inicial.
- Busca de rua `nacoes unidas`: mesma vista `X=-1693 Y=-762 ZOOM=2` nas duas versões.
- Busca `hebe camargo, 100`: mesma estimativa `X=-2144 Y=-2656 ZOOM=3` nas duas versões.
- Consulta sem resultado, fechamento da busca e preservação da vista.
- Três ciclos de inicialização e desligamento do Bluetooth. O novo `PDA HOME`
  saiu do app, enquanto `PDA KEY 27` continua representando Escape remoto.
- Salvamento, `PDA RESTART`, reconexão USB e restauração do Diagnóstico.
- Após reiniciar: `READY=1 STORAGE=1 DIRTY=0`, geração 2232, contagens
  preservadas e dez relatórios de jogos novamente idênticos aos originais.

## Tempos observados no aparelho

| Consulta | Firmware anterior | Firmware otimizado |
| --- | ---: | ---: |
| `nacoes unidas` | 2.625 ms | 938 ms |
| `hebe camargo, 100` | 1.109 ms | 954 ms |

Valores de uma execução por consulta, medidos no computador entre o envio do
Enter por USB e a confirmação `PDA INPUT OK`, com a tela real em uso. Incluem
latência USB e processamento de interface; não isolam o algoritmo e não são
um teste estatístico de desempenho. As coordenadas retornadas foram comparadas.

Na construção inicial dos vetores, os relatórios `MAP RENDER` registraram os
mesmos 6.595 candidatos e 14.229 traços: 99 ms antes e 93 ms depois. Outras
vistas também mantiveram contagens de geometria; isso não mede FPS nem a
percepção da rolagem por uma pessoa.

## Limites da validação

O GPS respondeu NMEA em RX44/9600, com antena reportada como OK. Depois da
atualização havia checksum válido, nenhum erro de UART/overflow e nenhuma
posição válida (`FIX=0`, zero satélites). Seguir um GPS em movimento e o
ajuste automático da hora não puderam ser confirmados nesta sessão.

O Bluetooth iniciou, anunciou/entrou em pareamento e desligou. Esta execução
não confirmou digitação ou movimento HID recebido por um host pareado.
Os comandos USB exercitam as rotas da interface; não comprovam a operação
manual das teclas físicas, trackball, touch ou qualidade visual do display.
O roteiro não gerou alarmes para evitar alterar o temporizador do usuário;
o fechamento de alertas no Bluetooth continua coberto pelo simulador.

Os logs completos, relatórios JSON e saída do esptool estão em
`build/device-validation-2026-11-03/` nesta máquina. Os roteiros pontuais dessa
sessão foram removidos na organização das ferramentas; os resultados foram
preservados. O console mantido para inspeções manuais é `tools/device.py`:

```powershell
python tools/device.py --port COM4 status
python tools/device.py --port COM4 games
python tools/device.py --port COM4 bt-peers
python tools/device.py --port COM4 bt-log
```

Essas consultas não repetem a automação histórica inteira. Para os testes de
regressão mantidos e o simulador, consulte [Ferramentas](../tools/README.md).
