"""Exercise chat CRUD on one uniquely named test note; never edits user records.

Creates, reads, edits, renames and deletes only the synthetic note. Exercises
timer control only when there is no running/paused timer. Event writes are
covered by native tests; this script does not change the device clock.
"""
import argparse,json,sys,uuid
from pathlib import Path
from device_connection import Connection,fields

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM4');parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    title='Teste USB'+uuid.uuid4().hex[:10];renamed=title+' final'
    with (args.output/'serial.txt').open('w',encoding='utf-8') as log:
        c=Connection(args.port,log)
        def key(code):c.command('PDA KEY '+str(code),'PDA INPUT OK')
        def text(s):
            assert len(s.encode())<=80
            for ch in s:key(ord(ch))
        def ask(s):text(s);key(13)
        def state():
            result,_=c.command('PDA STATUS\nPDA MEM','MEM DONE')
            return fields(next(s for s in result.splitlines() if s.startswith('PDA READY=')))
        def audio():
            result,_=c.command('PDA AUDIO STATUS','AUDIO ENABLED=')
            return fields(next(s for s in result.splitlines() if s.startswith('AUDIO ENABLED=')))
        def games():
            result,_=c.command('PDA GAMES','GAME I=9')
            return [s for s in result.splitlines() if s.startswith('GAME I=')]
        try:
            before=state();before_games=games();assert len(before_games)==10
            timer_before=audio()
            if timer_before['NOTIFY']:key(13)
            c.command('PDA HOME','PDA HOME OK');c.command('PDA HOME','PDA HOME OK')
            key(4);text('assistente');key(13);key(4)
            ask('nota: '+title);assert state()['NOTES']==before['NOTES']
            ask('sim');assert state()['NOTES']==before['NOTES']+1
            position=before['NOTES']+1
            ask('minhas notas');ask('le nota '+str(position))
            ask('edita essa nota: Texto atualizado do teste USB');ask('sim')
            ask('renomeia essa nota para '+renamed);ask('sim')
            ask('le nota '+str(position))
            ask('apaga essa nota');ask('cancelar')
            assert state()['NOTES']==before['NOTES']+1
            ask('exclui nota '+str(position));ask('sim')
            assert state()['NOTES']==before['NOTES']
            ask('sim');assert state()['NOTES']==before['NOTES']
            ask('minhas notas');ask('meus eventos')
            ask('exclui evento 4294967295');ask('sim')
            assert state()['EVENTS']==before['EVENTS']
            timer_changed=None;timer_cancelled=None
            if not timer_before['TIMER_RUNNING'] and not timer_before['TIMER_MS']:
                ask('timer de 30 segundos');ask('sim');started=audio()
                assert started['TIMER_RUNNING']==1 and 0<started['TIMER_MS']<=30000
                ask('altera o timer para 5 minutos');preview=audio()
                assert 0<preview['TIMER_MS']<=30000
                ask('sim');timer_changed=audio()
                assert timer_changed['TIMER_RUNNING']==1 and 299000<timer_changed['TIMER_MS']<=300000
                ask('cancela o timer');ask('cancelar');assert audio()['TIMER_RUNNING']==1
                ask('cancela o timer');ask('sim');timer_cancelled=audio()
                assert timer_cancelled['TIMER_RUNNING']==0 and timer_cancelled['TIMER_MS']==0 and timer_cancelled['LATCHED']==0
            key(4);ask('minhas notas');c.command('PDA SAVE','PDA SAVED');c.command('PDA MEM CHECK','MEM CHECK OK=1')
            after=state()
            for name in ['NOTES','CONTACTS','EVENTS','SHEETS','DRAWINGS']:assert after[name]==before[name]
            assert games()==before_games
            report={'test_note':title,'renamed_note':renamed,'test_note_removed':True,
                    'before':before,'after':after,'timer_before':timer_before,'timer_changed':timer_changed,
                    'timer_cancelled':timer_cancelled,'game_states_unchanged':True,'chat_left_open':True,
                    'event_writes_on_device':False,'event_crud_covered_by_native_tests':True,
                    'test_note_position':position,'selected_and_deleted_by_position':True,'notes_list_left_visible':True}
            (args.output/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2))
        finally:c.port.close()

if __name__=='__main__':main()
