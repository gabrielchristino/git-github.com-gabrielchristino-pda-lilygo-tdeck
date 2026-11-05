# Aprendizados consolidados

Este documento preserva as decisões das rodadas de IA e integração. Protótipos,
modelos antigos e relatórios repetidos foram removidos; o procedimento mantido
está em [IA.md](IA.md). Os logs locais ficam fora do Git em `build/`.

## Modelo pequeno e funções do produto

- Um classificador de 33 mil parâmetros cabe com folga e reconhece intenções;
  mais treino nesse formato não o transforma em um gerador de conversa livre.
- Um protótipo generativo em espanhol mostrou a diferença entre caber no S3 e
  ser útil em português. Foi substituído pelo classificador próprio; seus pesos,
  runtime e variantes não participam mais do produto.
- Notas, consultas, calendário e timers ganharam utilidade com APIs locais,
  interpretação de argumentos e diálogo. Cálculos de datas usam matemática,
  não exemplos memorizados pelo modelo.
- O modelo pode recusar uma frase que a gramática explícita atende. A rede não
  é a autorização de escrita: todo argumento precisa ser validado pelo serviço.

## Qualidade dos dados e das métricas

- Treino, calibração e avaliação precisam ser separados antes dos ajustes.
  Exemplos vistos para corrigir falhas viram regressão conhecida.
- Separar frases idênticas não separa necessariamente famílias, paráfrases ou
  representações: a preparação mascara nomes, números e conteúdo de notas.
- Mais exemplos sintéticos parecidos não equivalem a mais diversidade real.
  Abreviações, negativas e comandos compostos exigiram tratamento explícito.
- A confiança de 0,99 não significa 99% de acerto. É necessário reportar
  precisão entre aceitos, cobertura dos válidos e aceitações fora de escopo.
- A revisão atual fez 37/39 aceitos corretos no conjunto de 112 frases, com
  37/48 válidos reconhecidos e 2/64 negativos aceitos. Não demonstrou 99% de
  confiabilidade geral. A calibração é um conjunto diferente e mais otimista.
- Python e C++ devem concordar em características e probabilidades; só comparar
  rótulos pode esconder erro de preparação. Na consolidação, os 24.396 vetores
  e máscaras de política dos três conjuntos ficaram byte a byte iguais, e o
  treino repetido gerou o mesmo binário.

## Execução, identidade e persistência

- O texto original é preservado para nomes, telefones, durações e notas. Tokens
  mascarados pelo classificador não podem servir como argumentos.
- Variações como `crie um evento` e datas por extenso pertencem ao interpretador.
  Retreinar a classe AGENDA não corrige sozinho a extração desses argumentos.
  Se faltar horário, o diálogo guarda título/data e pergunta antes da prévia;
  nunca escolhe meia-noite ou trata `sim` como preenchimento de um dado faltante.
- Sinônimos e cortesias são tratados na cabeça do comando. Não se deve aplicar
  substituição global ao corpo de uma nota ou aos nomes; uma correspondência
  exata de título/contato tem preferência sobre remover `por favor` no fim.
- Consultas de agenda/timer precisam consumir a frase inteira. Encontrar as
  palavras `amanhã` ou `timer` não basta para aceitar qualificadores ou ações
  adicionais. Consultas explícitas de bateria/hora funcionam sem depender da rede.
- Alterar/excluir requer prévia concreta e confirmação consumida uma única vez.
  Uma falha de gravação não pode provocar duplicação ao repetir `sim`.
- IDs internos são estáveis e não reutilizados, inclusive após exclusões. A
  interface usa posições 1, 2, 3 da última listagem; um mapa em RAM resolve cada
  posição para o ID. Se a lista mudar, pede atualização, sem trocar o alvo.
- A confirmação guarda ID e campos esperados. Se o documento mudar, recusa o
  pedido antigo. Editores salvos do documento afetado são invalidados para não
  restaurar um rascunho antigo sobre a alteração do chat.
- Se salvar falhar, a alteração permanece na RAM, marcada como pendente, e o
  usuário é avisado. Os snapshots duplos existentes continuam cuidando de CRC,
  escrita interrompida e recuperação.
- O Assistente não grava seu ID 13 como app ativo no schema antigo, que aceita
  até 12. Conversa, prévias, contexto e paginação ficam na RAM; textos confirmados
  passam a integrar seus documentos.

## Memória e firmware

- O T-Deck medido tem flash de 16 MB e heap PSRAM de cerca de 8 MiB. DMA, pilhas
  e certas operações precisam da SRAM interna; ela não é somável livremente à PSRAM.
- O firmware usa uma única aplicação factory de 12,5 MiB, sem OTA. NVS e LittleFS
  mantêm endereços. Atualizar o app não requer apagar a flash ou o filesystem.
- Em ensaios com mapa/Bluetooth, restaram cerca de 7,20 MiB de PSRAM; o maior
  bloco contíguo chegou a 6,50 MiB. Uma reserva de 4,5 MiB passou nesses ensaios,
  mas não é um orçamento garantido para um gerador de linguagem.
- O orçamento inicial sugerido para novos experimentos é até 2,5 MiB somando
  pesos e buffers, sujeito a nova medição com dados grandes. Tokenizador,
  ativações, contexto e pilhas também custam memória.
- O mapa pode causar intervalos longos de loop; as medições precisam separar
  custo do modelo, transporte, renderização e salvamento. Médias não substituem
  os maiores intervalos observados. `profile_memory.py` continua reproduzível.

## Entrada, tela, USB e áudio

- A área do chat aceita 80 bytes; caracteres acentuados podem usar mais de um.
  Limites de caracteres e bytes são diferentes. A validação preserva rascunhos
  UTF-8 e divide leituras sem cortar uma sequência multibyte.
- WASD deve escrever texto no chat antes de passar pela navegação global. O
  mesmo evento Activate atende Enter e clique curto da bola; modais consomem o
  evento para não acionar a tela de baixo.
- O LVGL usado não tem Flex habilitado. As telas usam coordenadas e testes de
  renderização reais em 320×240, incluindo fontes e callbacks de toque.
- O USB CDC no Windows pode aguardar quando uma resposta termina exatamente no
  limite de pacote de 64 bytes. Diagnósticos usam flush e um pacote curto final;
  os testes leem até marcadores explícitos.
- O silêncio reportado no aparelho tinha a preferência Som desligada. Diagnóstico
  precisa distinguir configuração, entrega de PCM e escuta real.
- Áudio é enviado em blocos não bloqueantes, com retomada de escrita parcial,
  silêncio final e erro observável. Não se apaga a DMA antes de terminar o som.
  Dispensar o modal do timer para a repetição e esvazia o áudio imediatamente.
- A escuta no alto-falante confirmou os bipes e a parada ao fechar o modal;
  contadores de bytes por si só não provam saída acústica.

## Datas e testes físicos

- Datas incompletas precisam de relógio válido e fuso local. Um horário antigo
  salvo não é a data atual após perder alimentação.
- Intervalos e somas excluem o dia inicial. Dias úteis significam segunda a
  sexta sem feriados; essa convenção aparece na resposta.
- Para dia/mês sem ano, a consulta/soma usa a próxima ocorrência, incluindo hoje.
  Para intervalos sem ano, o início usa o ano atual e o fim avança se necessário.
  Datas explícitas não são reinterpretadas para outro ano.
- O cálculo foi comparado com `datetime` do Python e com uma referência lenta
  de dias úteis; casos de fevereiro, virada de ano e fins de semana são essenciais.
- Os ensaios físicos criam registros sintéticos únicos quando precisam escrever;
  removem apenas seus próprios registros e comparam os dez estados de jogos.
  Não alteram eventos reais para testar reagendamento nem substituem timers
  existentes. Cada ferramenta documenta alterações de relógio/som que pode fazer.
