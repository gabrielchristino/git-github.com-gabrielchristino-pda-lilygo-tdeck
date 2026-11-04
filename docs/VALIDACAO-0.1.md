# Validação da base 0.1

Data: 19 de outubro de 2026. Alvo: T-Deck original.

## Compilação do firmware

`platformio run -e tdeck`: **SUCCESS**.

| Componente | Versão observada |
| --- | --- |
| Plataforma espressif32 | 6.3.0 |
| Arduino-ESP32 | 2.0.9 |
| LVGL | 8.4.0 |
| TFT_eSPI | 2.5.43 |
| SensorLib | 0.2.0 |

Relatório do linker:

- RAM estática: 22.900 bytes de 327.680 (7,0%).
- Flash do aplicativo: 527.301 bytes de 6.553.600 (8,0%).
- `firmware.bin`: 527.664 bytes.
- Buffer de desenho alocado em runtime: 12.800 pixels RGB565, 25.600 bytes em PSRAM.

RAM estática não inclui os objetos LVGL, pilhas e demais alocações de runtime.
Estes números não são uma medição de pico de memória ou consumo elétrico.

Binário: `.pio/build/tdeck/firmware.bin`.

SHA-256:

```text
B689FB25A7DF51177FC3915AF05662AE6D3BCE076C8BD24EB7EF5863CD209981
```

## Entrada

`tests/run.ps1`: **passou**, compilado com C++11 e `-Wall -Wextra -Werror`.

Cobertura de comportamento:

- Oscilação do contato ao pressionar/soltar sem cliques extras.
- Clique curto reconhecido apenas após soltar.
- Clique longo retorna início uma única vez; soltar não ativa um item.
- Próximo clique funciona depois de um clique longo.
- Debounce e clique longo durante rollover de `millis()` de 32 bits.
- Loop atrasado: uma pressão longa não vira clique curto por falta de amostras.

## Interface no computador

Compilação nativa do mesmo `src/ui/shell.cpp`, catálogo e biblioteca LVGL local,
com framebuffer em memória e tick portável. O executável confere o título esperado
após abrir Notas pelo teclado, Diagnóstico pela navegação, voltar por callback de
touch e abrir Agenda pelo callback de um botão.

As cinco capturas foram geradas; início, busca, descrições e diagnóstico foram
inspecionados visualmente. A inspeção encontrou texto com contraste insuficiente
nas descrições; a cor de texto do conteúdo foi corrigida e as capturas regeneradas.

- [Início](previews/home.png).
- [Busca por notas](previews/search.png).
- [Escopo de notas](previews/notes-plan.png).
- [Diagnóstico](previews/diagnostic.png).
- [Escopo de agenda](previews/agenda-plan.png).

Na prévia, `Touch: detectado` vem de um parâmetro simulado para renderizar esse
estado. Isso não confirma que um aparelho físico foi detectado.

## Gravação no T-Deck

Em 19/10/2026, por solicitação do usuário:

- Dispositivo identificado na COM4: ESP32-S3 revisão v0.2, flash de 16 MB.
- Firmware gravado com esptool 4.5.1 por USB, a 921.600 baud.
- Bootloader em `0x0`, partições em `0x8000`, inicialização OTA em `0xe000`
  e aplicativo em `0x10000`.
- A ferramenta confirmou o hash de cada região gravada e executou o reset.
- O usuário confirmou que o launcher do PDA apareceu na tela.
- A leitura de backup foi interrompida a pedido do usuário; nenhum backup
  completo foi produzido.

Uma tentativa de captura do log após reset não retornou mensagens; a confirmação
de exibição do launcher foi fornecida pelo usuário. Isso não valida os periféricos.

## Validação física restante

Ainda é necessário verificar na placa:

- Cores da tela, alinhamento e detecção do touch.
- Teclado real e os bytes de números, operadores e símbolos.
- Direções, sensibilidade e ruído da bola; clique curto/longo em uso real.
- Disponibilidade de PSRAM, estabilidade e uso de memória em runtime.

Persistência e aplicativos pessoais ainda não estão implementados e não foram
testados. Os projetos de referência permanecem intactos; `git status --short`
do repositório `t-deck-os` estava vazio ao final da validação.

