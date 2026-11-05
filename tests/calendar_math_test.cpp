#include "pda/calendar_math.h"
#include <cstdio>
#include <cstdlib>
using namespace pda;
static unsigned checks=0;
static void check(bool ok,const char *why) {++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static bool has(const std::string &s,const char *text) {return s.find(text)!=std::string::npos;}
static CivilTime date(const char *s) {CivilTime d;check(parseCivil(s,d),s);return d;}
int main() {
    auto today=date("04/11/2026 12:00");
    const auto ask=[&](const char *s){return calendarReply(s,true,today);};
    for(const char *s:{"quando cai dia 8 de novembro de 2026","quando cai dia 8 de novembro","quando cai dia 8","que dia da semana e 08/11/2026?"})
        check(ask(s)=="08/11/2026: domingo.",s);
    check(has(ask("do dia 1 de novembro ate 25 de dezembro sao quantos dias"),"54 dias corridos"),"requested interval uses current year even when start already passed");
    check(has(ask("dia 8 de novembro mais 5 dias corridos"),"13/11/2026 (sexta-feira)"),"requested calendar addition");
    check(has(ask("dia 8 de novembro mais 5 dias uteis"),"13/11/2026 (sexta-feira)"),"requested business addition");
    check(has(ask("dia 6 de novembro mais 5 dias corridos"),"11/11/2026"),"calendar and business days differ across weekend");
    check(has(ask("dia 6 de novembro mais 5 dias uteis"),"13/11/2026"),"business addition crosses weekend");
    check(has(ask("dia 6 de novembro mais 1 dia util"),"09/11/2026"),"Friday plus one business day");
    check(has(ask("dia 7 de novembro mais 1 dia util"),"09/11/2026"),"Saturday plus one business day");
    check(has(ask("dia 9 de novembro menos 1 dia util"),"06/11/2026"),"Monday minus one business day");
    check(has(ask("dia 8 de novembro menos 5 dias uteis"),"02/11/2026"),"Sunday minus five business days ignores holidays explicitly");
    check(has(ask("dia 8 de novembro mais 0 dias uteis"),"08/11/2026"),"zero business days preserves weekend date");
    check(has(ask("quantos dias entre 01/11/2026 e 25/12/2026"),"54 dias corridos"),"interval prefix variant");
    check(has(ask("entre 01/11/2026 e 25/12/2026 quantos dias uteis"),"40 dias úteis"),"business interval excludes weekends only");
    check(has(ask("de 25/12 ate 01/01 sao quantos dias"),"7 dias corridos"),"yearless range crosses year");
    check(has(ask("de 28/02/2028 ate 01/03/2028 sao quantos dias"),"2 dias corridos"),"leap-year interval");
    check(has(ask("de 28/02/2026 ate 01/03/2026 sao quantos dias"),"1 dia corrido"),"common-year interval");
    check(has(ask("de 01/11/2026 ate 01/11/2026 sao quantos dias"),"0 dias corridos"),"same date interval is zero");
    check(has(ask("de 25/12/2026 ate 01/11/2026 sao quantos dias"),"anterior"),"explicit reversed interval not silently changed");
    check(has(ask("dia 28 de fevereiro de 2028 mais 1 dia"),"29/02/2028"),"leap-day addition");
    check(has(ask("dia 31 de dezembro de 2026 mais 1 dia"),"01/01/2027"),"year rollover addition");
    check(has(ask("dia 1 de janeiro de 2027 menos 1 dia"),"31/12/2026"),"negative year rollover");
    today=date("09/11/2026 12:00");
    check(ask("quando cai dia 8")=="08/12/2026: terça-feira.","day-only selects next month");
    check(ask("quando cai dia 8 de novembro")=="08/11/2027: segunda-feira.","day/month selects next year");
    today=date("08/11/2026 23:00");check(ask("quando cai dia 8")=="08/11/2026: domingo.","today is eligible");
    today=date("04/11/2026 12:00");check(ask("quando cai dia 31")=="31/12/2026: quinta-feira.","day-only skips nonexistent month day");
    check(ask("quando cai dia 29 de fevereiro")=="29/02/2028: terça-feira.","yearless leap day finds next leap year");
    today=date("31/12/2026 12:00");check(ask("quando cai dia 8")=="08/01/2027: sexta-feira.","next occurrence crosses year");
    for(const char *s:{"quando cai dia 8","quando cai dia 8 de novembro","dia 8 de novembro mais 5 dias uteis","do dia 1 de novembro ate 25 de dezembro sao quantos dias"})
        check(has(calendarReply(s,false,CivilTime()),"ajuste"),s);
    check(calendarReply("quando cai dia 8 de novembro de 2026",false,CivilTime())=="08/11/2026: domingo.","explicit date independent of device clock");
    check(has(calendarReply("de 01/11/2026 ate 25/12 sao quantos dias",false,CivilTime()),"54 dias"),"explicit start anchors missing end year without clock");
    check(has(calendarReply("de 01/11 ate 25/12/2026 sao quantos dias",false,CivilTime()),"54 dias"),"explicit end anchors missing start year without clock");
    for(const char *s:{"quando cai dia 31 de novembro de 2026","quando cai dia 29 de fevereiro de 2026","quando cai dia 0","quando cai dia 32","quando cai dia 8 de novmento","quando cai 08/11/2100","quando cai 08/11/1999"}) {
        const auto r=ask(s);check(!r.empty()&&!has(r,": domingo.")&&!has(r,": segunda-feira."),s);
    }
    for(const char *s:{"dia 8 de novembro mais 5.5 dias uteis","dia 8 de novembro mais 36526 dias","dia 8 de novembro mais 5 horas","dia 8 de novembro mais 5 dias e apaga notas"})
        check(!has(ask(s)," = "),s);
    check(has(ask("dia 31 de dezembro de 2099 mais 1 dia"),"fora do intervalo"),"upper date bound");
    check(has(ask("dia 1 de janeiro de 2000 menos 1 dia util"),"fora do intervalo"),"lower date bound");
    today=date("31/12/2099 12:00");check(has(ask("quando cai dia 8"),"fora do intervalo"),"implicit date cannot exceed supported years");
    for(const char *s:{"quanto e 12 vezes 5.3","12/2 + 5","quanto e 12/2+5","nota: dia 8 mais 5 dias","qual o telefone de Tamires?","timer de 5 segundos"})
        check(ask(s).empty(),s);
    // Independent slow reference for the optimized whole-week business-day path.
    const auto base=date("01/11/2026 00:00");
    for(int weekday=0;weekday<7;++weekday) for(int count=-25;count<=25;++count) {
        const auto start=addDays(base,weekday);auto expected=start;
        int left=count<0?-count:count;const auto step=count<0?-1:1;
        while(left) {expected=addDays(expected,step);if(weekDay(expected)!=0&&weekDay(expected)!=6) --left;}
        const auto request="dia "+formatCivil(toUtc(start,0),0,false)+(count<0?" menos ":" mais ")+std::to_string(count<0?-count:count)+" dias uteis";
        const auto expectedDate=formatCivil(toUtc(expected,0),0,false);
        check(has(calendarReply(request,false,CivilTime()),expectedDate.c_str()),request.c_str());
    }
    printf("calendar: %u checks passed\n",checks);
}
