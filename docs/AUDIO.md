# Alertas sonoros do T-Deck

Timers concluídos e avisos da agenda usam `System::notify`, que respeita
**Ajustes → Som**. Com som desligado, a notificação visual continua disponível.
As teclas e a navegação não emitem cliques. Para testar a mesma saída dos
alertas, use **Ajustes → Testar som** com **Som: ligado**.

Quando um timer termina, os três bipes se repetem a cada **1,5 segundo** até
dispensar o modal por **OK**, **Enter** ou **clique curto no trackball**. A
dispensa também interrompe imediatamente qualquer áudio ainda na DMA. Os avisos
da agenda e o teste de som continuam emitindo uma sequência por acionamento.

## Implementação

O aviso anterior enviava uma única amostra de 40 ms, com amplitude de pico 1500,
sem verificar o resultado da escrita. Isso produzia um aviso curto e fraco e
não permitia distinguir silêncio configurado de erro no driver.

`src/hal/audio.cpp` substitui esse caminho por três tons de 1 kHz/160 ms,
com pausas de 100 ms e 40 ms finais de silêncio. A amplitude de pico é 10000,
com envelope nas bordas. O PCM de 16 bits/16 kHz é duplicado nos dois canais.
O buffer tem 128 quadros estéreo (512 bytes), sem uma tarefa dedicada.

O envio ocorre em `powerTick`, em até quatro escritas não bloqueantes por
iteração. Escritas parciais são retomadas sem perder bytes. Pedidos simultâneos
são agrupados; desativar Som interrompe a fila. Falhas de inicialização/escrita
e ausência de progresso por um segundo são registradas no diagnóstico. A DMA
não é apagada ao terminar de enfileirar o aviso, para não cortar o som final.

A repetição do timer é controlada por `System`, ligada à notificação pendente,
sem persistir outro campo no snapshot. `dismissNotification` limpa a repetição
e chama `Platform::stopAlert`, que esvazia o áudio. O mesmo caminho atende toque,
teclado e trackball. Desativar Som silencia a saída, mantendo o modal visível.

Os pinos são BCLK=7, WS=5 e DOUT=6, conforme o
[exemplo oficial SimpleTone da LilyGO](https://github.com/Xinyuan-LilyGO/T-Deck/blob/master/examples/SimpleTone/SimpleTone.ino).
O controle de alimentação GPIO10 já é ativado pelo HAL do PDA. A implementação
não inicializa microfone nem roteia MCLK ao GPIO0 (botão da bola).

## Diagnóstico USB

- `PDA AUDIO STATUS`: habilitação, inicialização, reprodução, último erro,
  pedidos, padrões enfileirados, bytes aceitos pelo driver, estado do timer e
  existência de notificação e `LATCHED` (aviso repetitivo do timer pendente).
  Não revela o texto dos documentos ou avisos.
- `PDA AUDIO TEST`: emite o mesmo aviso ou responde `AUDIO TEST MUTED` se
  o som estiver desligado. Não muda a preferência.

`COMPLETED` conta padrões completamente enfileirados; a última DMA ainda pode
estar saindo pelo I2S nesse instante. Esses contadores verificam o caminho de
software, não medem a pressão sonora ou a conexão física do alto-falante.

## Testes

`tests/audio_test.cpp` cobre configuração e pinos, erro de instalação,
recuperação, escritas parciais/timeouts, limite de trabalho por iteração,
PCM estéreo idêntico, amplitude, pausas, silêncio final e cancelamento.
`tests/core_test.cpp` verifica expiração, repetição, silêncio e cancelamento,
incluindo a ausência de novo som após dispensar o modal. A prévia LVGL testa
**Testar som** nos dois estados e a dispensa do timer por toque em OK, Enter e
ativação gerada pelo clique do trackball.

```powershell
./tests/run.ps1
python tools/test_sound_device.py --port COM4 --output build/sound-device-test
```

O teste físico pode ligar o som se estiver desativado. Dispara um teste direto
e, se não houver timer em andamento, cria e confirma `timer de 5 segundos` pelo
chat. Confere expiração, repetição até o reconhecimento, parada por Enter e
ativação equivalente ao trackball, bytes aceitos, ausência de erro,
integridade do heap e preservação de documentos e jogos. Deixa o chat aberto.
A confirmação de que os bipes foram ouvidos depende de quem está com o aparelho.

Os bipes e a interrupção ao fechar o modal foram confirmados pelo usuário no
aparelho. O diagnóstico do silêncio inicial e as decisões adotadas estão em
[Aprendizados](APRENDIZADOS.md).
