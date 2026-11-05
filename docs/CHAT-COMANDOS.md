# Notas, agenda e timer pelo chat

Os comandos abaixo funcionam no app Assistente, sem abrir os aplicativos.
As leituras aparecem na tela. Alterações e exclusões mostram uma prévia e exigem
**sim** ou **confirmar**. **cancelar** / **não** descarta o pedido. A confirmação
é consumida uma única vez, inclusive quando a gravação falha.

**ajuda**, **comandos** e **o que você faz?** mostram o resumo das capacidades.
Use **mais** para avançar ou **ajuda notas**, **ajuda agenda**, **ajuda timer**,
**ajuda contatos** para abrir um assunto. A ajuda só consulta informações;
não confirma nem descarta uma alteração pendente.

## Variações de linguagem

Todas as funções abaixo passam pela interpretação explícita, mesmo quando o
modelo recusa a frase. Prefixos como `pode`, `poderia`, `por favor`, `gostaria de`
e `quero` são aceitos antes do comando. Não significam que qualquer frase livre
será compreendida. Perguntas, consultas e durações também aceitam pontuação final
e formas como `por favor`/`pra mim` no fim; conteúdo de documentos é preservado.

| Função | Outras formas aceitas |
| --- | --- |
| Criar nota | `crie uma nota sobre comprar pão`, `guarde uma nota: Café`, `registre uma anotação dizendo comprar leite` |
| Listar/ler | `pode mostrar minhas notas?`, `quais são as minhas notas?`, `leia a primeira nota`, `visualize nota 1` |
| Buscar | `pesquise notas Café`, `encontre eventos Dentista` |
| Editar | `modifique nota 1: novo texto`, `atualize o texto da nota 1 para texto novo` |
| Renomear | `renomeie nota 1 para Compras`, `mude o título da nota 1 para Compras` |
| Excluir | `remova a primeira nota`, `delete evento 1`, `elimine nota Compras` |
| Evento/lembrete | `crie um evento Comprar pão amanhã às 9h`, `me lembre de comprar leite hoje às 18h` |
| Remarcar | `reagende evento 1 para amanhã`, `reprograme evento 1 para dia 6 de novembro de 2026 às 10h` |
| Agenda | `pode mostrar minha agenda de amanhã?`, `quais compromissos eu tenho amanhã?`, `qual o próximo compromisso da minha agenda?` |
| Criar contato | `cadastre contato Oficina com o telefone 11999998888`, `adicione o contato Oficina: 11999998888` |
| Consultar contato | `me mostre o contato de Tamires`, `qual é o telefone de Tamires?`, `qual o número de celular de Tamires?` |
| Iniciar timer | `inicie uma contagem regressiva de vinte e cinco segundos`, `programe um temporizador para uma hora e meia` |
| Consultar timer | `mostre o temporizador`, `quanto tempo restou no temporizador?`, `quanto falta para o timer acabar?` |
| Alterar/cancelar timer | `mude a duração do timer para um minuto e meio`, `cancele o temporizador por favor` |
| Contas | `multiplique 12 por 5,3`, `divida 288 por 32,66`, `some 12 com 5,3`, `subtraia 5 de 12` |
| Calendário | `em qual dia da semana cai 08/11/2026?`, `por favor, que dia cai 8 de novembro de 2026?` |
| Bateria/hora | `pode mostrar o nível da bateria?`, `qual a porcentagem de bateria?`, `poderia me dizer a hora?` |
| Ajuda/paginação | `o que posso pedir?`, `mostre os comandos`, `continue`, `próxima` |

Ordinais de primeira a décima também funcionam após o tipo, como `leia nota
segunda`, usando a última lista exibida. `confirmo` confirma uma prévia, e
`deixa pra lá` cancela. Criar/alterar continua exigindo confirmação; `sim` não
preenche um horário que falta. Uma solicitação de horário para reagendamento
continua ligada ao mesmo ID e recusa a confirmação se o evento mudou.

`hoje` e `amanhã` usam a data local do PDA e exigem relógio válido. Datas por
extenso também podem vir depois do título. Sem horário, o chat pergunta; sem
dia/mês/ano suficientes, pede uma data completa. Os demais cálculos de calendário
mantêm [suas próprias regras de datas incompletas](CHAT-CALENDARIO.md).

O campo permanece limitado a 80 bytes. Não são feitas chamadas, abertura de apps,
consultas externas ou operações em listas de compras. Pedidos compostos ou com
restrições não atendidas não são reduzidos silenciosamente a uma duração válida.
Nomes/títulos com palavras como `por favor` têm preferência sobre a interpretação
de cortesia quando já existe uma correspondência exata.

## Notas

| Pedido | Resultado |
| --- | --- |
| `minhas notas`, `lista notas`, `mostra minhas notas` | Lista títulos por posição (1, 2, 3...) |
| `busca notas Compras` | Busca pelo título, sem distinguir acentos/maiúsculas |
| `le nota Compras`, `lê a nota Compras` | Mostra o texto da nota encontrada |
| `le nota 1` | Abre a leitura pela posição mostrada na lista |
| `mais`, `continua` | Próxima parte da leitura ou da lista |
| `edita nota 1: novo texto` | Substitui todo o texto, mantendo o título |
| `altera nota Compras para novo texto` | Outra forma de substituir o texto |
| `renomeia nota 1 para Compras da semana` | Altera só o título |
| `apaga nota 1`, `exclui nota Compras` | Exclui somente a nota selecionada |

Após ler ou alterar uma nota, também pode usar `edita essa nota: novo texto`,
`renomeia essa nota para Compras` ou `apaga essa nota`. Após criar uma nota
confirmada, ela também se torna a referência atual. Uma listagem ou uma busca
ambígua remove essa referência para não escolher um item por engano.

Os textos de leitura são divididos em páginas de até 384 bytes, respeitando
limites UTF-8; as listas mostram até cinco itens por página. `mais` continua.
O texto original fica intacto. Editar pelo chat substitui o corpo completo:
a prévia avisa isso. Para edições longas, use o editor do app; o campo do chat
continua limitado a 80 bytes por mensagem.

## Eventos e lembretes da agenda

Para criar, use `evento Comprar pão 05/11/2026 09:00` ou
`crie um evento para dia 5 de novembro de 2026 para comprar pão`.
No segundo caso, o chat pergunta o horário: responda `9h` ou `09:00` e só
depois confirme a prévia com `sim`. Também aceita o horário na frase:
`crie um evento para dia 5 de novembro de 2026 às 9h para comprar pão`.
O título preserva acentos; a data precisa incluir ano. Não escolhe meia-noite
quando o horário falta. `cancelar`, `não` ou limpar descarta o pedido inteiro.

| Pedido | Resultado |
| --- | --- |
| `meus eventos`, `lista eventos`, `ler os eventos` | Lista os eventos já armazenados, inclusive passados |
| `busca eventos Dentista` | Busca pelo título |
| `le evento Dentista`, `le evento 1` | Mostra título, data, fim, lembrete e detalhes |
| `agenda hoje`, `agenda amanha`, `qual meu proximo evento?` | Consultas por período, numeradas por posição |
| `remarca evento 1 para 20/11/2026 10:30` | Altera o início, mantendo duração e lembrete |
| `altera evento Dentista para 20/11/2026 10:30` | Outra forma de remarcar |
| `renomeia evento 1 para Consulta` | Altera só o título |
| `edita evento 1: levar exames` | Substitui todos os detalhes |
| `exclui evento 1`, `apaga evento Dentista` | Exclui somente o evento selecionado |

Depois de ler/alterar um evento, `esse evento` funciona como referência: por
exemplo, `remarca esse evento para 20/11/2026 10:30` ou `exclui esse evento`.
`lembrete` e `compromisso` também são aceitos como nomes do tipo de registro.
As mesmas APIs cuidam dos eventos dos apps Agenda e Calendário.

Listar, ler, renomear, editar detalhes e excluir registros existentes funciona
sem relógio válido. Criar ou remarcar exige relógio ajustado e data futura
completa, no fuso configurado no PDA. Ao remarcar, o aviso é rearmado; ao editar
só título/detalhes, seu estado anterior é preservado. Eventos com rascunho de
edição pendente devem ser concluídos na Agenda antes de editar/excluir pelo chat.

## Timer

| Pedido | Resultado |
| --- | --- |
| `timer`, `status do timer`, `quanto tempo falta no timer?` | Consulta tempo restante ou estado pausado |
| `altera o timer para 5 minutos` | Substitui o tempo restante após confirmação |
| `muda o temporizador para meio minuto` | Outra forma de ajustar a duração |
| `cancela o timer`, `cancelar o temporizador` | Cancela a contagem e zera o restante após confirmação |

A alteração não soma tempo: `5 minutos` define cinco minutos restantes. O timer
continua contando durante a revisão; cancelar a prévia preserva a contagem.
Se estava pausado, alterar a duração o mantém pausado. Se não existir timer,
o chat pede que crie um com `timer de 5 minutos`. Se outra ação substituir,
pausar ou concluir o timer antes de confirmar, o pedido antigo é recusado.

O aviso de timer concluído continua repetindo até fechar o modal com OK, Enter
ou trackball, conforme [o funcionamento do áudio](AUDIO.md).

## Seleção e persistência

- Os números agora são posições na última lista exibida: `le nota 1`,
  `edita nota 2: novo texto`, `exclui evento 1`. Use números sem #.
  Ao atualizar a lista, ela começa em 1, mesmo que os IDs internos tenham saltos.
  Buscas e consultas da agenda também começam em 1 nos seus próprios resultados;
  a paginação continua a numeração (6, 7...).
- Se a quantidade ou ordem dos registros mudar, o chat pede uma nova listagem
  antes de aceitar outra posição. Use `minhas notas` ou `meus eventos` para
  atualizar. Uma confirmação pendente continua vinculada ao item original.
- Título exato tem preferência. Se não houver exato, uma correspondência parcial
  única pode ser usada. Dois resultados nunca são resolvidos escolhendo o primeiro.
  O chat mostra candidatos e pede a posição desejada. Não há exclusão em lote.
- O pedido guarda o ID e uma cópia dos campos usados na prévia. Se o item mudar
  ou desaparecer enquanto aguarda `sim`, a alteração/exclusão é recusada.
- Limpar o chat ou reiniciar descarta contexto, paginação e confirmação pendente.
  Cancelar um pedido limpa esse contexto, mantendo a numeração da última lista.
  Nenhuma mensagem vira treino automaticamente.
- Se o editor salvo apontava para o documento alterado/excluído, sua sessão é
  retornada à lista, evitando restaurar um rascunho antigo sobre o resultado novo.
  Outros documentos e suas sessões não são modificados.
- A gravação usa os snapshots existentes. Em caso de falha, o chat informa que
  a operação ficou só na RAM e o sistema tentará gravar novamente. Exclusão não
  oferece desfazer no app.

## Validação e próxima etapa

Os pesos v6 continuam iguais: essa rodada amplia a interpretação explícita e
as APIs, sem atribuir novas capacidades generativas ao classificador. A suíte
dos serviços tem 373 verificações, cobrindo calendário, posições (inclusive buscas e
consultas por data), mudanças de ordem, leitura/paginação, UTF-8, títulos
duplicados, contexto, confirmação única, edições/exclusões obsoletas, rascunhos,
reagendamento, timer alterado/pausado/concluído, falha de gravação e reboot.
A prévia LVGL executa criação, leitura, edição, renomeação e exclusão de notas
e eventos e alteração/cancelamento de timer, com o modelo real carregado.
As consultas explícitas de bateria/hora também foram verificadas com o
classificador desligado. Na versão ampliada, passaram 32 entradas no chat do
T-Deck, com prévias canceladas, heap íntegro, contagens de documentos e os dez
jogos preservados. O Assistente ficou aberto e limpo para uso.

```powershell
./tests/run.ps1
python tools/test_chat_crud_device.py --port COM4 --output build/chat-crud-device-test
```

O teste físico cria uma nota com nome único, edita/renomeia e exclui apenas essa
nota, conferindo as contagens finais e os estados dos jogos. Exercita timers
somente se não houver contagem em andamento/pausada. Não muda o relógio nem os
eventos reais; operações de escrita em eventos ficam nos testes nativos.

Para ampliar os comandos, treinar o reconhecedor e avaliar capacidades futuras,
consulte [Modelo e treinamento](IA.md). As decisões de implementação estão em
[Aprendizados](APRENDIZADOS.md).
