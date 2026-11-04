# Diagnóstico do canal esquerdo

## Estado atualizado em 31/10/2026

O usuário informou que reparou o canal esquerdo e que agora todas as direções
funcionam. O problema do trackball está resolvido conforme teste de uso relatado
pelo proprietário. Os ensaios abaixo ficam como histórico anterior ao reparo.

19/10/2026. O usuário relatou movimento para direita/cima/baixo, com falha à
esquerda inclusive no Diagnóstico. A leitura anterior mostrava apenas o movimento
líquido enviado à interface, insuficiente para separar ausência de sinal de
cancelamento entre direções.

## Instrumentação instalada

- Contadores cumulativos independentes para esquerda, direita, cima e baixo nas
  interrupções CHANGE, antes de subtrair os movimentos opostos.
- Amostragem direta dos mesmos pinos no loop, com contagem independente das
  mudanças de nível.
- Exibição do nível do GPIO1 e das contagens no app Diagnóstico, atualizada
  mesmo quando não existe movimento líquido para despachar.
- Comando USB `PDA BALL` / `tools/device.py ball`.
- Navegação e mapa de pinos mantidos; não foram inventados movimentos à esquerda.

A pinagem confere com os exemplos oficiais: esquerda G04/GPIO1, direita
G02/GPIO2, cima G01/GPIO3 e baixo G03/GPIO15; todos INPUT_PULLUP.

## Ensaios com movimentos feitos pelo usuário

Contadores por interrupção / polling, cumulativos desde o boot:

| Etapa | Esquerda | Direita | Cima | Baixo |
| --- | --- | --- | --- | --- |
| Inicial | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 |
| Após teste solicitado só à esquerda | 0 / 0 | 248 / 118 | 57 / 35 | 64 / 44 |
| Após teste solicitado só à direita | 0 / 0 | 417 / 201 | 58 / 36 | 68 / 48 |

O usuário confirmou a conclusão de ambos os testes. O GPIO1 permaneceu alto nas
amostras consultadas. Polling pode perder pulsos rápidos; as contagens menores
que as das interrupções são esperadas e não são usadas para mover a interface.

O sinal esquerdo não apresentou transições observáveis por nenhum dos dois
caminhos. Outros canais produziram pulsos. Isso aponta para uma falha no sinal
do canal esquerdo/conjunto do trackball, mas não identifica mecanicamente a causa
(rolagem, sensor, conexão, trilha ou entrada GPIO). Não é prova de qual componente
deve ser substituído.

Correção posterior do usuário: no acompanhamento da tela, mover à esquerda não
acionava o indicador direito. Os totais acima foram coletados entre consultas e
não devem ser interpretados como prova de que o canal direito detecta esquerda.

Na inspeção física, o usuário encontrou o CI da posição UZ4 com solda solta e
enviou fotos. O reparo ficou pendente por falta de estação de solda. Ainda não
foi possível verificar eletricamente o componente após ressoldagem. Para permitir
o uso enquanto isso, calendário, desenho e grade da planilha passaram a aceitar
W/A/S/D como movimento. Nos editores essas letras continuam sendo texto normal.

## Validação do firmware de diagnóstico

- Compilação ESP32-S3 passou: RAM estática 23.788 bytes, app 730.305 bytes.
- SHA-256 da imagem: `3D8ED2F8EE59306FD6D426EB6F7A65A384B22ED96FFFA1CAAF15F92C94FE515E`.
- Imagem gravada somente no aplicativo; hash verificado pela ferramenta.
- Placa respondeu com STORAGE=1, uma nota e uma planilha preservadas.
- Suíte nativa de apps passou; teste adicional confirma que contadores brutos
  aparecem sem depender de um evento de movimento líquido.
- Captura do layout: [Diagnóstico](previews/diagnostic.png). A captura usa valores
  de teste; os resultados físicos são os da tabela acima.

## Referências primárias

- [Pinagem oficial](https://raw.githubusercontent.com/Xinyuan-LilyGO/T-Deck/master/examples/UnitTest/utilities.h).
- [Leitura da bola no teste oficial](https://raw.githubusercontent.com/Xinyuan-LilyGO/T-Deck/master/examples/UnitTest/UnitTest.ino).
