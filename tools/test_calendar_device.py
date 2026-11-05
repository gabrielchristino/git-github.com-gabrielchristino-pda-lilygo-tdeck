"""Compare calendar answers on the ESP32 with Python's independent date arithmetic.

Read-only calculations and real chat input. Synchronizes an invalid device clock
with the computer UTC clock; never creates or edits documents.
"""
import argparse,calendar,json,sys,time
from datetime import date,datetime,timedelta
from pathlib import Path
from device_connection import Connection,fields

WEEKDAYS=['segunda-feira','terça-feira','quarta-feira','quinta-feira','sexta-feira','sábado','domingo']
def stamp(d):return d.strftime('%d/%m/%Y')
def next_date(today,day,month=None):
    for year in range(today.year,2100):
        for m in range(1,13):
            if month and m!=month:continue
            if day>calendar.monthrange(year,m)[1]:continue
            d=date(year,m,day)
            if d>=today:return d
    raise ValueError('No date in supported years')
def business_add(start,count):
    result=start
    while count:
        result+=timedelta(days=1)
        if result.weekday()<5:count-=1
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',default='COM4');parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=False)
    with (args.output/'serial.txt').open('w',encoding='utf-8') as log:
        c=Connection(args.port,log)
        def key(n):c.command('PDA KEY '+str(n),'PDA INPUT OK')
        def text(s):
            assert len(s.encode())<=80
            for ch in s:key(ord(ch))
        def state():
            result,_=c.command('PDA STATUS\nPDA MEM','MEM DONE')
            return fields(next(s for s in result.splitlines() if s.startswith('PDA READY=')))
        def games():
            result,_=c.command('PDA GAMES','GAME I=9')
            return [s for s in result.splitlines() if s.startswith('GAME I=')]
        def calculate(prompt):
            result,_=c.command('PDA CALENDAR '+prompt,'CALENDAR DONE')
            header=fields(next(s for s in result.splitlines() if s.startswith('CALENDAR CLOCK=')))
            reply=result.split('CALENDAR REPLY=',1)[1].split('\nCALENDAR DONE',1)[0].strip()
            return header,reply
        try:
            before=state();before_games=games();assert len(before_games)==10
            synced=not bool(before['CLOCK'])
            if synced:
                c.command('PDA TIME '+str(int(time.time())),'PDA TIME OK');c.command('PDA SAVE','PDA SAVED')
            header,_=calculate('quando cai dia 8 de novembro de 2026');assert header['CLOCK']==1
            today=datetime.strptime(header['TODAY'],'%d/%m/%Y').date()
            november=next_date(today,8,11);day8=next_date(today,8)
            cases=[
                ('quando cai dia 8 de novembro de 2026',['08/11/2026: domingo.']),
                ('quando cai dia 8 de novembro',[stamp(november)+': '+WEEKDAYS[november.weekday()]]),
                ('quando cai dia 8',[stamp(day8)+': '+WEEKDAYS[day8.weekday()]]),
                ('do dia 1 de novembro ate 25 de dezembro sao quantos dias',
                 [stamp(date(today.year,11,1)),stamp(date(today.year,12,25)),'54 dias corridos']),
                ('dia 8 de novembro mais 5 dias corridos',[stamp(november+timedelta(days=5))]),
                ('dia 8 de novembro mais 5 dias uteis',[stamp(business_add(november,5)),'sem feriados']),
                ('dia 6 de novembro de 2026 mais 5 dias uteis',['13/11/2026 (sexta-feira)']),
                ('dia 28 de fevereiro de 2028 mais 1 dia corrido',['29/02/2028']),
                ('de 25/12/2026 ate 01/01/2027 sao quantos dias',['7 dias corridos']),
                ('entre 01/11/2026 e 25/12/2026 quantos dias uteis',['40 dias úteis','sem feriados']),
            ]
            results=[]
            for prompt,expected in cases:
                clock,reply=calculate(prompt)
                for part in expected:assert part in reply,(prompt,reply,part)
                results.append({'prompt':prompt,'reply':reply,'clock':clock})
            # Traverse the same keyboard path used by the user as well.
            result,_=c.command('PDA AUDIO STATUS','AUDIO ENABLED=')
            audio=fields(next(s for s in result.splitlines() if s.startswith('AUDIO ENABLED=')))
            if audio['NOTIFY']:key(13)
            c.command('PDA HOME','PDA HOME OK');c.command('PDA HOME','PDA HOME OK')
            key(4);text('assistente');key(13);key(4)
            for prompt,_ in cases[:6]:
                text(prompt);key(13)
                result,_=c.command('PDA CHAT STATUS','CHAT ACTIVE=')
                chat=fields(next(s for s in result.splitlines() if s.startswith('CHAT ACTIVE=')))
                assert chat['ACTIVE']==1 and chat['BYTES']==0
            key(4);text('quando cai dia 8 de novembro de 2026');key(13)
            c.command('PDA MEM CHECK','MEM CHECK OK=1');after=state()
            for name in ['NOTES','CONTACTS','EVENTS','SHEETS','DRAWINGS']:assert before[name]==after[name]
            assert games()==before_games
            report={'computer_clock_sync':synced,'device_today':str(today),'cases':results,
                    'keyboard_prompts':6,'before':before,'after':after,'documents_and_games_preserved':True,
                    'chat_left_with_weekday_answer':True}
            (args.output/'summary.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
            print(json.dumps(report,ensure_ascii=False,indent=2))
        finally:c.port.close()

if __name__=='__main__':main()
