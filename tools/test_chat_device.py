"""Exercise the physical chat through the same input events as the keyboard.

Uses synthetic prompts only. Leaves an empty Assistente open for manual testing.
"""
import argparse,json,sys
from pathlib import Path
from device_connection import Connection,fields

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM4');parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    with (args.output/'serial.txt').open('w',encoding='utf-8') as log:
        c=Connection(args.port,log)
        def key(code): c.command('PDA KEY '+str(code),'PDA INPUT OK')
        def type_text(text):
            for ch in text:key(ord(ch))
        def stats():
            text,_=c.command('PDA CHAT STATUS','CHAT ACTIVE=')
            return fields(next(s for s in text.splitlines() if s.startswith('CHAT ACTIVE=')))
        def launch():
            c.command('PDA HOME','PDA HOME OK');c.command('PDA HOME','PDA HOME OK')
            key(4);type_text('assistente');key(13)
            assert stats()['ACTIVE']==1
        def state():
            text,_=c.command('PDA STATUS\nPDA MEM','MEM DONE')
            return fields(next(s for s in text.splitlines() if s.startswith('PDA READY=')))
        def games():
            text,_=c.command('PDA GAMES','GAME I=9')
            return [s for s in text.splitlines() if s.startswith('GAME I=')]
        def memory():
            text,_=c.command('PDA MEM','MEM DONE')
            return {name:fields(next(s for s in text.splitlines() if s.startswith('MEM '+name+' '))) for name in ['INTERNAL','PSRAM','PROBE']}
        records=[]
        try:
            before=state();before_games=games();assert len(before_games)==10
            launch();key(4);assert stats()['MESSAGES']==1
            type_text('wasd$');assert stats()['BYTES']==5
            key(4);key(13);assert stats()['MESSAGES']==1
            # A date without a time must remain a dialogue, not a midnight write.
            for text in ['crie um evento para dia 5 de novembro de 2026 para comprar pao',
                         'sim','25:00','9h','cancelar','sim']:
                type_text(text);key(13);actual=stats()
                assert actual['BYTES']==0 and actual['ACTIVE']==1,(text,actual)
                records.append({'text':text,'expected_label':None,**actual})
            assert state()['EVENTS']==before['EVENTS']
            key(4)
            for text,label in [('qnt % de bateria ainda tem?',1),('q horas sao',2),('timer 45s',6),('anote comprar madeira',7),('abrir calc',8),('abre whats',0)]:
                type_text(text);key(13);actual=stats()
                assert actual['LABEL']==label and actual['BYTES']==0 and actual['ACTIVE']==1,(text,actual)
                records.append({'text':text,'expected_label':label,**actual})
                if label in (6,7):type_text('cancelar');key(13)
            # Exercise the deterministic service grammars independently of the
            # neural label. Writes are previewed and cancelled, never confirmed.
            for text in ['timer de 5 segundos','cria um timer de cinco segundos',
                         'temporizador para 2 minutos',
                         'quanto e 12 vezes 5.3','agenda hoje','qual meu proximo evento?',
                         'qual o telefone de Teste Inexistente?',
                         'contato Teste USB: 11999998888',
                         'evento Teste USB 18/11/2026 09:00',
                         'por favor, crie uma nota sobre comprar pao',
                         'cadastre contato Oficina com o telefone 11999998888',
                         'inicie uma contagem regressiva de vinte e cinco segundos',
                         'pode mostrar minhas notas?',
                         'pode mostrar minha agenda de amanha?',
                         'pode mostrar o nivel da bateria?',
                         'poderia me dizer a hora?',
                         'multiplique 12 por 5,3',
                         'em qual dia da semana cai 08/11/2026?',
                         'o que posso pedir?',
                         'nao cria um timer de 30s']:
                assert len(text.encode('utf-8'))<=80
                type_text(text);key(13);actual=stats()
                assert actual['BYTES']==0 and actual['ACTIVE']==1,(text,actual)
                records.append({'text':text,'expected_label':None,**actual})
                type_text('cancelar');key(13)
                type_text('sim');key(13) # no pending action can survive cancellation
            type_text('nota: Teste USB descartado');key(13);key(4)
            type_text('sim');key(13) # clearing must discard the proposed write
            type_text('rascunho');c.command('PDA HOME','PDA HOME OK');launch();assert stats()['BYTES']==8
            key(4);type_text('x'*81);assert stats()['BYTES']==80;key(4)
            for _ in range(8):type_text('q horas sao');key(13)
            assert stats()['MESSAGES']==12
            warm=memory()
            for _ in range(20):type_text('q horas sao');key(13)
            assert stats()['MESSAGES']==12
            after=memory();assert after['INTERNAL']['FREE']>=warm['INTERNAL']['FREE']-4096,(warm,after)
            c.command('PDA MEM CHECK','MEM CHECK OK=1')
            key(4);c.command('PDA HOME','PDA HOME OK')
            end=state()
            for name in ['NOTES','CONTACTS','EVENTS','SHEETS','DRAWINGS']:assert end[name]==before[name]
            assert games()==before_games
            launch();assert stats()['MESSAGES']==1 and stats()['BYTES']==0
            report={'prompts':records,'bounded_history':12,'physical_ascii_limit':80,'memory_warm':warm,'memory_after_stress':after,
                    'before':before,'after':end,'game_states_unchanged':True,'document_counts_unchanged':True,
                    'chat_left_open':True,'write_actions_executed':0}
            (args.output/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2))
        finally:c.port.close()

if __name__=='__main__':main()
