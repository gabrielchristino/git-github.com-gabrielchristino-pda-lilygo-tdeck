# Persistência e recuperação

A fonte de verdade em runtime é `pda::State`, compartilhada pelos aplicativos.
Ela contém documentos, preferências, estado dos temporizadores e uma sessão com
app/documento, vista, edição, cursor e seleção. Notas, contatos e células recebem
as alterações do editor. Compromissos guardam também campos de rascunho para
preservar datas incompletas sem agendar um horário inválido.

As dez partidas de jogos também pertencem ao estado persistente: tabuleiros,
seleções, bandeiras, pistas/valores, pontuações, recordes, fase, estado de vitória
ou derrota, gerador aleatório e uma jogada de desfazer quando disponível.

## Formato

Snapshots binários portáveis, little-endian e sem dumps de structs/padding:
`PDASNAP1`, versão do schema, geração de 64 bits, tamanho, payload e CRC32 sobre
cabeçalho e payload. O decoder verifica tamanho, limites das coleções e strings,
IDs únicos, coordenadas, preferências e integridade antes de substituir a RAM.
Arquivos de schema desconhecido ou duas cópias inválidas ficam protegidos contra
sobrescrita. O usuário pode inicializar a memória com confirmação em Ajustes.

A primeira revisão com jogos gravava **schema 2** e continuava lendo **schema 1**. O payload
anterior é mantido como prefixo; as partidas foram acrescentadas ao fim. Ao ler
schema 1, os jogos começam vazios e os dados pessoais são preservados. O próximo
salvamento grava schema 2 na cópia inativa. Firmwares antigos que só leem schema 1
não conseguem ler as novas cópias depois da conversão; não fazer downgrade sem
suporte ao formato novo. A migração é coberta por fixture nativa de schema 1.

A revisão com **Forca** grava **schema 3**, que acrescenta o quinto jogo, e lê os
schemas 1 e 2. Ao ler schema 2, preserva as quatro partidas existentes e inicia
a Forca vazia. Há fixtures de ambos os formatos anteriores, incluindo jogos já
iniciados. A palavra, letras tentadas, erros, resultado e vitórias são persistentes.

A ampliação para dez jogos grava **schema 4**, com leitura dos schemas 1, 2 e 3.
Os cinco jogos iniciais mantêm seus campos e índices. Os cinco novos ficam no fim;
campos de texto de partida passam a aceitar até 512 bytes para frotas, corpo da
cobra e mapas. Ao migrar, os jogos novos começam vazios. Há fixtures de todos os
formatos anteriores e roundtrip dos dez jogos com seus dados auxiliares.

A Cobrinha salva corpo, fruta, direção e comando pendente, mas sua simulação
retoma pausada ao reabrir a tela. Sair para o menu/início ou receber um lembrete
pausa a partida antes do próximo passo. Não há avanço enquanto outro app está aberto.

A flash usa o mapa de partições já instalado na base 0.1. A partição `spiffs`
em `0xc90000`, tamanho `0x360000`, é montada como LittleFS. Não há mudança de
partições nem dependência de NVS para documentos nesta versão.

## Duas cópias

`pda-a.bin` e `pda-b.bin` alternam. Uma gravação substitui apenas a cópia inativa;
a cópia ativa anterior permanece. A implementação verifica escrita, `fflush`,
`fsync`, fechamento e leitura de volta antes de avançar a geração em memória.
Ao iniciar, o leitor escolhe a geração válida mais recente. Se uma cópia estiver
incompleta, recupera a outra. O filesystem fornece sua própria proteção contra
interrupções, mas apenas testes físicos podem confirmar os efeitos de queda de
tensão no aparelho específico.

A área só é formatada automaticamente quando está **inteiramente em 0xFF**.
Se não montar e houver conteúdo desconhecido, exige ação explícita de preparo.
Falha de montagem, espaço, escrita ou verificação deixa o estado em RAM e mostra
o erro. Não é feita formatação automática como tentativa de corrigir corrupção.

## Frequência e energia

A primeira alteração inicia um período pendente. Salvar após 750 ms de inatividade
ou após 5 s durante mudanças contínuas. Criar, excluir, confirmar e trocar de
tela solicitam checkpoint urgente; voltar ao início força salvamento. Falhas
são tentadas novamente com intervalo mínimo de 5 s, além de ações explícitas.
O aviso `salvando…` significa que ainda pode haver alterações somente em RAM.

As gravações precisam ocorrer enquanto existe alimentação. Não há garantia de
preservar a última tecla se a bateria for removida antes do checkpoint. Um estado
marcado `salvo` passou pela leitura de verificação. O teste em computador simula
escrita truncada e verifica a recuperação do último estado válido.

Cronômetro e timer têm checkpoints a cada 5 s. Após boot, cronômetro volta
pausado. O timer usa seu prazo UTC quando o relógio é confiável; caso contrário,
restaura pausado com o restante do último checkpoint. Uma data salva não é
restaurada como se fosse hora atual. Estado de lembrete já disparado também é
salvo, evitando repetição normal a cada troca de tela/reinício.

A redução/apagamento da tela economiza iluminação e mantém o loop ativo. Esta
versão não usa deep sleep nem promete autonomia específica. O interruptor e o
circuito da bateria continuam responsáveis pela alimentação física.

## Atualizações

`tools/flash.ps1` grava apenas o app em `0x10000`. Não apaga a partição de dados,
a tabela de partições, o bootloader nem os arquivos salvos. Mudanças futuras de
schema precisam de migração explícita; não reduzir validações para montar dados
incompatíveis. O limite de snapshot é 1,4 MB para permitir duas cópias na partição.
