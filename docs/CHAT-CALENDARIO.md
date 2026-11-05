# Cálculos de calendário no chat

Digite **ajuda calendario** para ver exemplos no aparelho. Os cálculos são
locais e somente de leitura; não cadastram eventos nem exigem confirmação.
Os pesos do reconhecedor v6 permanecem iguais.

Com o relógio em **04/11/2026**, os exemplos pedidos respondem:

| Pergunta | Resultado |
| --- | --- |
| `quando cai dia 8 de novembro de 2026` | 08/11/2026: domingo |
| `quando cai dia 8 de novembro` | 08/11/2026: domingo |
| `quando cai dia 8` | 08/11/2026: domingo |
| `do dia 1 de novembro ate 25 de dezembro sao quantos dias` | 54 dias corridos |
| `dia 8 de novembro mais 5 dias corridos` | 13/11/2026, sexta-feira |
| `dia 8 de novembro mais 5 dias uteis` | 13/11/2026, sexta-feira |

Os dois últimos resultados coincidem porque 08/11/2026 é domingo. Para ver a
diferença, `dia 6 de novembro de 2026 mais 5 dias corridos` dá 11/11/2026;
com `5 dias uteis`, dá 13/11/2026.

## Datas incompletas

- Uma data completa usa exatamente o ano informado, inclusive para datas passadas.
- Em consultas de dia da semana e soma/subtração, sem ano é escolhida a primeira
  ocorrência **de hoje em diante**, incluindo hoje.
- `quando cai dia 8 de novembro`, em 09/11/2026, considera 08/11/2027.
- `quando cai dia 8`, em 09/11/2026, considera 08/12/2026.
- Dia 31 em novembro pula para dezembro; 29 de fevereiro sem ano procura o
  próximo ano bissexto válido. Não transforma uma data impossível em outro dia.
- A resposta sempre mostra a data completa que foi considerada.

O relógio do PDA e o fuso configurado em Ajustes definem hoje. Se o relógio
estiver inválido, as consultas que dependem de mês/ano atual pedem ajuste.
Datas totalmente informadas continuam funcionando. Se o aparelho perder a
hora após ficar sem alimentação, ajuste em Relógio ou aguarde uma hora válida
do GPS. Um horário antigo salvo não é tratado como se fosse a data atual.

## Intervalos e contagem

São aceitas formas como `de 01/11/2026 ate 25/12/2026 sao quantos dias`,
`quantos dias entre 01/11/2026 e 25/12/2026` e
`entre 01/11/2026 e 25/12/2026 quantos dias uteis`.

Quando nenhum ano é informado, o início usa o ano atual (e o mês atual se
também faltar mês), mesmo que já tenha passado. O fim sem ano usa a primeira
ocorrência igual ou posterior ao início. Assim, novembro a dezembro no mesmo
ano dá 54 dias, e 25 de dezembro a 1 de janeiro cruza para o ano seguinte.
Se uma das datas informa o ano, ela pode servir de referência para a outra,
sem precisar do relógio. Um intervalo totalmente explícito com o fim anterior
ao início é recusado; o cálculo não troca silenciosamente as datas.

A contagem **exclui o dia inicial**. A mesma data até ela própria dá zero.
Somar um dia a uma sexta-feira dá sábado em dias corridos ou segunda-feira
em dias úteis. Zero dias mantém a data, mesmo que seja fim de semana.

## Dias úteis e limites

**Dias úteis significam segunda a sexta, sem considerar feriados nacionais,
estaduais, municipais ou pessoais.** A resposta informa essa convenção. Por
isso, o intervalo 01/11/2026 a 25/12/2026 tem 40 dias úteis nesta modalidade,
incluindo feriados que caiam de segunda a sexta. Calendários de feriados
podem ser acrescentados em uma etapa posterior.

Também é possível subtrair, por exemplo `dia 9 de novembro de 2026 menos 1 dia
util`, ou escrever datas numéricas: `dia 08/11/2026 mais 5 dias`.
Sem o qualificativo, `dias` significa dias corridos.

Os dias e quantidades devem ser números inteiros em algarismos. Os nomes dos 12 meses são
aceitos com ou sem acentos. Anos: 2000 a 2099; quantidade por operação:
0 a 36525 dias. Resultados fora do intervalo são recusados. O campo do chat
continua limitado a 80 bytes. Frases com palavras extras não consumidas pedem
reformulação em vez de ignorar parte das instruções.

## Implementação e validação

`src/core/calendar_math.cpp` interpreta as frases e usa o calendário gregoriano
de `datetime.cpp`. Diferenças são calculadas entre datas civis, sem transformar
mudanças de horário/fuso em dias fracionários. Somas úteis pulam semanas
completas e verificam apenas os dias restantes, sem laços proporcionais ao
número solicitado. O contexto e os documentos do usuário não são modificados.

`tests/calendar_math_test.cpp` passou em 421 verificações, incluindo os exemplos,
datas incompletas, viradas de mês/ano, bissextos, intervalos, limites, relógio
inválido e comparação da soma útil otimizada com uma referência passo a passo.
As 373 verificações do assistente incluem fuso local, acentos, ausência de
cadastros, conteúdo literal de notas e proteção de confirmação pendente.
A prévia LVGL verificou respostas na interface com o modelo real carregado.

```powershell
./tests/run.ps1
python tools/test_calendar_device.py --port COM4 --output build/calendar-device-test
```

`PDA CALENDAR <frase>` é um diagnóstico de leitura. Recebe uma frase normalizada
(minúsculas, sem acentos), informa a data local/fuso usados e a resposta do mesmo
módulo de cálculo, terminando com `CALENDAR DONE`. O teste compara essas
respostas com o módulo `datetime` do Python e percorre a entrada do chat por USB.
Confere documentos, dez jogos e integridade do heap; pode sincronizar um relógio
inválido com o UTC do computador. Não grava eventos de teste.

