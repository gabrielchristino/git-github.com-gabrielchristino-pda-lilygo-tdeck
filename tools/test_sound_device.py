"""Test audible alerts and a confirmed five-second chat timer on a real T-Deck.

May enable sound if muted. Never replaces an existing running timer or changes
documents/games. An electrical/software pass still needs human listening.
"""
import argparse,json,sys,time
from pathlib import Path
from device_connection import Connection,fields

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM4')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    with (args.output/'serial.txt').open('w',encoding='utf-8') as log:
        c=Connection(args.port,log)
        def key(code):c.command('PDA KEY '+str(code),'PDA INPUT OK')
        def text(s):
            for ch in s:key(ord(ch))
        def audio():
            result,_=c.command('PDA AUDIO STATUS','AUDIO ENABLED=')
            return fields(next(s for s in result.splitlines() if s.startswith('AUDIO ENABLED=')))
        def home():
            c.command('PDA HOME','PDA HOME OK');c.command('PDA HOME','PDA HOME OK')
        def launch(name):home();key(4);text(name);key(13)
        def idle(previous):
            deadline=time.monotonic()+4
            while time.monotonic()<deadline:
                a=audio()
                if not a['PLAYING'] and a['COMPLETED']>previous:return a
                time.sleep(.1)
            raise AssertionError(('audio stalled',audio()))
        def state():
            result,_=c.command('PDA STATUS\nPDA MEM','MEM DONE')
            return fields(next(s for s in result.splitlines() if s.startswith('PDA READY=')))
        def games():
            result,_=c.command('PDA GAMES','GAME I=9')
            return [s for s in result.splitlines() if s.startswith('GAME I=')]
        try:
            before=state();before_games=games();initial=audio()
            # Dismiss any earlier notification before operating the launcher.
            if initial['NOTIFY']:key(13)
            if not initial['ENABLED']:
                launch('ajustes');text('s');key(13)
                assert audio()['ENABLED']==1
            baseline=audio()
            c.command('PDA AUDIO TEST','AUDIO TEST REQUESTED')
            direct=idle(baseline['COMPLETED'])
            assert direct['ERROR']==0 and direct['INIT']==1 and direct['REQUESTS']==baseline['REQUESTS']+1,direct
            assert direct['BYTES']-baseline['BYTES']==46080,direct
            time.sleep(.1);key(13)
            timer=None
            if not initial['TIMER_RUNNING']:
                launch('assistente');key(4);text('timer de 5 segundos');key(13)
                preview=audio();assert preview['TIMER_RUNNING']==0
                assert preview['REQUESTS']==direct['REQUESTS']
                text('sim');key(13);started=audio()
                assert started['TIMER_RUNNING']==1 and 0<started['TIMER_MS']<=5000,started
                deadline=time.monotonic()+8
                while time.monotonic()<deadline:
                    status=audio()
                    if not status['TIMER_RUNNING']:break
                    time.sleep(.25)
                timer=idle(direct['COMPLETED'])
                assert timer['ERROR']==0 and timer['NOTIFY']==1 and timer['TIMER_MS']==0,timer
                assert timer['REQUESTS']==direct['REQUESTS']+1 and timer['BYTES']-direct['BYTES']==46080,timer
                assert timer['LATCHED']==1
                time.sleep(2);repeated=audio()
                assert repeated['REQUESTS']>timer['REQUESTS'] and repeated['LATCHED']==1,repeated
                key(13);stopped=audio()
                assert stopped['LATCHED']==0 and stopped['PLAYING']==0 and stopped['NOTIFY']==0,stopped
                time.sleep(2);assert audio()['REQUESTS']==stopped['REQUESTS']
                key(4)
                # The USB key 0 maps to the same Activate event as a short ball click.
                text('timer de 1 segundo');key(13);text('sim');key(13)
                deadline=time.monotonic()+5
                while time.monotonic()<deadline:
                    ball=audio()
                    if ball['LATCHED']:break
                    time.sleep(.1)
                assert ball['LATCHED']==1,ball
                key(0);ball_stopped=audio()
                assert ball_stopped['LATCHED']==0 and ball_stopped['PLAYING']==0 and ball_stopped['NOTIFY']==0,ball_stopped
                time.sleep(2);assert audio()['REQUESTS']==ball_stopped['REQUESTS']
                key(4)
            else:launch('assistente');key(4)
            c.command('PDA MEM CHECK','MEM CHECK OK=1')
            after=state()
            for name in ['NOTES','CONTACTS','EVENTS','SHEETS','DRAWINGS']:assert before[name]==after[name]
            assert games()==before_games
            report={'initial_audio':initial,'direct_test':direct,'timer_test':timer,'before':before,'after':after,
                    'documents_and_games_preserved':True,'human_listening_required':True,'chat_left_open':True,
                    'timer_repeats_until_acknowledged':timer is not None,'enter_and_ball_activation_stop_audio':timer is not None}
            (args.output/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2))
        finally:c.port.close()

if __name__=='__main__':main()
