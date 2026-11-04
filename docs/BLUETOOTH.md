# Teclado e mouse Bluetooth

Abra **Bluetooth** no launcher do PDA. No PC ou celular, procure **T-Deck PDA**
nas configurações Bluetooth e confirme o pareamento. Aguarde **Conectado** na
tela do T-Deck antes de digitar. Não requer aplicativo receptor.
Se já houver algum vínculo salvo, abra **Aparelhos > Parear novo** antes de
procurar um segundo destino.

## Alternar entre celular e computador

1. Pareie o primeiro aparelho pelo Bluetooth do próprio PC/celular.
2. No T-Deck, toque em **Aparelhos**, depois em **Nome** na linha desse vínculo.
   Escolha **Computador**, **Celular** ou **Aparelho** para identificá-lo.
3. Volte à lista e use **Parear novo** para cadastrar o segundo aparelho pelo
   Bluetooth dele. Dê um nome ao novo vínculo da mesma maneira.
4. Para alternar, abra **Aparelhos** e selecione a linha do destino desejado.
   Retorne por **Teclado** e aguarde **Conectado** para digitar.

Os dois vínculos ficam salvos, com **uma conexão ativa por vez**. `*` identifica
o destino escolhido e `[on]` identifica a conexão pronta. Quando o destino
estiver desligado, fora de alcance ou sem aceitar reconexão, o T-Deck permanece
aguardando esse destino; não troca automaticamente para o outro. Alguns
sistemas exigem selecionar **Conectar** no próprio destino para retomar a
conexão. Selecionar uma linha no T-Deck não consegue ligar o Bluetooth do host.

Há até **três vínculos salvos**. Em **Nome > Esquecer**, confirme para remover
somente aquele vínculo. Para parear novamente do zero, remova também o
cadastro **T-Deck PDA** no Bluetooth do PC/celular. A operação não apaga os
documentos do PDA nem os demais vínculos.

A lista mostra identidades já pareadas, não uma varredura de aparelhos
próximos. O nome inicial é `Aparelho` com o final do endereço; os nomes
**Celular/Computador/Aparelho** são rótulos locais que você escolhe. O detalhe
mostra a identidade completa para distinguir aparelhos com nomes iguais.

O nome e o destino selecionado são salvos em NVS separadamente dos documentos.
Ao sair do app, os vínculos continuam guardados. Reabrir ou reiniciar mantém
o destino escolhido. **Parear novo** libera o cadastro de um aparelho ainda
desconhecido e rejeita a reconexão dos anteriores durante esse cadastro.
Após dois minutos sem um novo vínculo, retorna ao destino anterior, se houver.
Sair e reabrir o app também retorna ao destino salvo. Sem vínculo anterior,
o app continua disponível para o primeiro pareamento enquanto estiver aberto.

Na tela de aparelhos, WASD/bola e Tab navegam, Enter/clique escolhe, e Backspace
ou Escape volta; essas entradas não são enviadas ao host. A confirmação de
**Esquecer** começa com **Cancelar** selecionado. Na tela de teclado, as mesmas
teclas voltam a funcionar como entradas remotas.

Configure o teclado físico do aparelho de destino como **Inglês (US)** para
que os símbolos correspondam ao teclado do T-Deck. Acentos e layout ABNT2 ainda
não são implementados. O perfil é BLE HID: o destino precisa aceitar teclado
e mouse Bluetooth Low Energy. A compatibilidade real depende do aparelho e
ainda precisa ser testada com este firmware.

| Controle | Ação no aparelho conectado |
| --- | --- |
| Teclado | Letras, números, símbolos ASCII, Enter, Tab, Backspace e Escape |
| Shift + tecla física $ | Envia `=` |
| Bola | Move o ponteiro |
| Clique curto da bola | Clique esquerdo; solta um arrasto ativo |
| Botão Direito na tela | Clique direito |
| Botão Arrastar na tela | Alterna manter o botão esquerdo pressionado |
| Rolar + / Rolar - na tela | Rola para cima / baixo |
| Segurar a bola por 700 ms ou tocar na casa | Sai do app e desconecta |

O painel tem quatro abas touch:

- **Mouse:** controles do mouse e botões Ctrl+C, Ctrl+V, Ctrl+X, Ctrl+Z, Ctrl+A,
  Ctrl+S, Alt+Tab e Win (abre o menu do sistema).
- **Teclas:** Ctrl, Alt, Shift e Win combináveis, Esc, Tab, Backspace, Delete,
  Home, End, Page Up, Page Down e setas.
- **F1–F12:** doze teclas de função, Win, Print Screen, Insert e Pause.
- **Símbolos:** todos os 32 sinais de pontuação ASCII, incluindo `<`, `>`, `~`,
  `=`, colchetes, chaves, barra invertida e crase. **Mais** mostra os restantes;
  **Voltar** retorna à primeira página.

Na aba **Teclas**, toque em Ctrl/Alt/Shift/Win para marcar a combinação e depois
pressione uma tecla física ou uma tecla touch. Os modificadores se aplicam
somente à próxima tecla e então desmarcam. É possível combinar vários deles,
ou tocar novamente para cancelar. Eles permanecem marcados ao mudar de aba,
permitindo Alt + F4 ou Ctrl + Home. O rodapé mostra a combinação pendente.
Sair do app ou perder a conexão limpa a combinação. Nenhuma tecla modificadora
fica pressionada no PC enquanto você escolhe a próxima tecla no T-Deck.

O teclado original envia caracteres, sem eventos de pressionar/soltar cada
tecla. Shift é reconstruído para maiúsculas e símbolos. Bytes de controle
recebidos são convertidos em Ctrl+letra, exceto os controles dedicados acima;
Shift+$ (byte 0x04) é reservado para enviar `=`. Modificadores, setas e teclas
de função estão no painel touch. WASD sempre digita neste app.

O Bluetooth só anuncia enquanto o app está aberto. Após uma desconexão, volta
a anunciar para o destino escolhido. Para trocar, selecione outro vínculo em
**Aparelhos**; o firmware libera os reports de teclado/mouse e desconecta o
anterior antes de aceitar o próximo. O vínculo é salvo pelo
NimBLE em NVS; sair do app não apaga o pareamento. O pareamento usa bonding,
criptografia e o método Just Works, sem digitação de PIN.

No iPhone, o ponteiro requer AssistiveTouch. Consulte as
[instruções da Apple](https://support.apple.com/en-in/111775).
Para teclados físicos no Android, consulte o
[guia do Google](https://support.google.com/android/answer/14114879?hl=pt-BR).

## Compilar, instalar e validar

O build usa NimBLE-Arduino 1.4.3 fixado em `platformio.ini`, com apenas os papéis
de periférico/anunciante e uma conexão. Isso reduz o tamanho do firmware para
manter o mapa offline e a partição atual.
Os dados do mapa e os nomes usam a compactação sem perda descrita em
[Uso de memória](MEMORIA.md); coordenadas, nomes e endereços permanecem completos.

Para diagnóstico local por USB, `PDA BT PEERS` lista vínculos, identidades,
rótulos e seleção/conexão. `PDA BT SELECT identidade` escolhe um vínculo já
salvo, e `PDA BT MANAGE` abre a mesma tela de aparelhos quando o app Bluetooth
estiver aberto. `PDA HOME` sai do app inclusive quando Escape é uma tecla remota.
Esses comandos não fazem varredura e não geram teclas no host.

A revisão de destinos de 03/11/2026 compila com 5.221.377 bytes de aplicativo
(79,7% da partição) e 37.976 bytes de RAM estática. A imagem de 5.221.744 bytes
foi gravada na COM4 com hash verificado:
`b96a81ee2b68f638af7543074a88600d323169b47d35320bd8386e49a9d14095`.
Os testes de `bluetooth_peers_test.cpp` exercitam a implementação HAL com um
transporte simulado: troca de identidade, liberação dos reports, callbacks
atrasados, falha de gravação, endereços privados, persistência, limite de três
vínculos, timeout de cadastro e falha no filtro sem aceitar outro destino.
O simulador testa seleção, rótulos e exclusão com confirmação por teclado.
No aparelho, a lista dos dois vínculos antigos, seleção por identidade,
navegação e preferência após reiniciar passaram. Esses vínculos foram depois
apagados a pedido do usuário para refazer o cadastro; isso não comprova ainda
a reconexão alternada com dois hosts disponíveis.

```powershell
& C:/Users/gabri/.platformio/penv/Scripts/python.exe -m platformio run -e tdeck
./tools/flash.ps1 -Port COM4 -SkipBuild
```

Use a porta real do T-Deck. O script grava só o aplicativo, preservando os dados;
antes da atualização, salve a sessão no PDA.

Verificação no destino: parear; digitar `aAzZ09 !@#$% ()_+-=[]{};:'",.<>/?` em
um campo vazio; testar Enter/Tab/Backspace/Escape; mover nas quatro direções;
clicar, arrastar, rolar; sair; reabrir; reconectar e repetir após reiniciar.
Também teste uma desconexão durante arrasto: o destino deve liberar o mouse.

Os testes nativos cobrem o mapeamento de teclas, roteamento de entradas,
botões da tela, saída do modo remoto e persistência da sessão. Eles não validam
o rádio, o pareamento ou a recepção das notificações HID pelo destino.

## Validação em 02/11/2026

- Build final: sucesso; aplicativo de 6.515.533 bytes na partição de 6.553.600;
  imagem de 6.515.904 bytes; RAM estática de 37.824 bytes.
- SHA-256 da imagem: `F6F77C7230CF9BED4C96465B6DF11B66EA31D5998EB28F24C01BBBF5AD332E96`.
- `tests/run.ps1`: passou, incluindo mapa após compactação dos registros.
- Renderer nativo: todos os testes passaram, incluindo Bluetooth; tela conferida
  em `build/preview/bluetooth.png`.
- Sessão salva antes da instalação. Aplicativo gravado na COM4 em `0x10000`,
  com hash verificado pelo esptool. Partições de dados e bootloader preservados.
- Boot: READY=1, STORAGE=1; preservados 1 nota, 1 contato, 3 compromissos,
  0 planilhas e 0 desenhos. App Bluetooth aberto por comandos de entrada USB:
  APP=12, HOME=0, DIRTY=0, status **Aguardando conexão**, heap livre 219.348 bytes.
- Reinício por `PDA RESTART`: voltou ao app Bluetooth com armazenamento pronto,
  status **Aguardando conexão** e heap livre 219.340 bytes.
- Pareamento com PC/celular, teclas físicas, movimento físico da bola e recepção
  HID no destino ainda dependem de teste do usuário.

### Teste real com o PC após pareamento pelo usuário

O usuário pareou o T-Deck com este PC. O aparelho confirmou **Conectado**.
Um receptor WinForms local (`tools/BluetoothReceiver.cs`) registrou somente os
eventos entregues à sua própria janela, com foco conferido antes do envio.
`tools/test_bluetooth_pc.py` injetou entradas por USB no firmware: as entradas
passaram pelo app Bluetooth, rádio BLE e pilha HID do Windows até o receptor.

- Texto recebido exatamente: `TDeck Bluetooth aAzZ09 !@#$% ()_+-=[]{}<>/?`,
  seguido por Enter, Tab e `OK`. Backspace removeu corretamente o `X` de teste.
- Recebidos movimentos de mouse e eventos de pressionar/soltar botão esquerdo.
- Evidências locais: `build/bluetooth-pc-visible-test/text.txt` e `events.tsv`.
- Este teste confirma recepção HID real no Windows. Teclas e bola físicas,
  clique direito, arrasto, rolagem, reconexão e celular ainda precisam de testes
  reais; os botões adicionais foram verificados apenas nos testes de interface.

### Atalho para igual

Revisão de 02/11/2026: Shift+$ agora envia `=` no modo Bluetooth. O rodapé
mostra o atalho. O teste da interface confirmou a conversão do byte 0x04 para
o caractere `=`, e o firmware compilou com aplicativo de 6.515.545 bytes.
SHA-256 da imagem desta revisão:
`83B270CC62361583D947AC8E2C5BE7039195123CFA0C934BA571FBA79FE12F4E`.

### Painel touch de teclas especiais e símbolos

Revisão de 02/11/2026: quatro abas com mouse/atalhos, teclas, F1–F12 e dois
conjuntos de símbolos. O envio de cada tecla/chord HID é seguido por um report
zerado, inclusive quando a tecla é apenas Win. A seleção de modificadores fica
somente na interface até a próxima tecla; mouse e troca de abas não a consomem.

- Testes de interface passaram: todos os atalhos prontos, Ctrl+Shift+C,
  Ctrl+Alt+Del, Win+E, Alt+F4, cancelamento por segundo toque, liberação após a
  próxima tecla e limpeza ao sair ou perder conexão. Nenhum desses atalhos foi
  executado em aplicativos do PC durante esses testes: usou-se um receptor
  simulado dos reports HID.
- Todas as setas, teclas de navegação, F1–F12, Print Screen, Insert e Pause
  conferidas pelos códigos HID; símbolos `<`, `>`, `~`, `=`, `\`, `|`, `?` e `$`
  conferidos por callbacks touch. O mapa ASCII continua cobrindo os 32 sinais.
- Capturas e limites dos botões conferidos nas quatro abas e nas duas páginas
  de símbolos: `build/preview/bluetooth-panels.png`.
- Build: aplicativo de 6.517.661 bytes, imagem de 6.518.032 bytes e RAM estática
  de 37.872 bytes. SHA-256:
  `DA2A739259AB6CB895F9F6DF078B0FA17A7DECC97E8ADF434F77CBE6A19953B5`.
- Instalado na COM4 somente em `0x10000`, com hash verificado. Após reiniciar:
  READY=1, STORAGE=1, APP=12, DIRTY=0 e Bluetooth **Conectado**; preservados
  1 nota, 1 contato, 3 compromissos, 0 planilhas e 0 desenhos.
- O acionamento por touch físico e a recepção dos novos reports no PC/celular
  ainda dependem de teste real do usuário.
