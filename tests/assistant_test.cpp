#include "pda/assistant.h"
#include "pda/datetime.h"
#include <cstdio>
#include <cstdlib>
using namespace pda;
static unsigned checks=0;
static void check(bool ok,const char *why) {++checks;if(!ok){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static bool has(const std::string &s,const std::string &part) {return s.find(part)!=std::string::npos;}
class Memory : public StorageBackend {
public:
    std::vector<uint8_t> slots[2];bool fail=false;
    bool ready() const override{return true;}
    bool exists(int i) const override{return !slots[i].empty();}
    bool read(int i,std::vector<uint8_t> &out) override{out=slots[i];return !out.empty();}
    bool write(int i,const std::vector<uint8_t> &out) override{if(fail)return false;slots[i]=out;return true;}
    const char *description() const override{return "test";}
};
class Clock : public Platform {public:int64_t epoch=1793790000;int64_t utc() const override{return epoch;}};
static void languageVariationTests() {
    Memory mem;Clock clock;System sys;sys.begin(&mem,&clock,0);Assistant chat;PdaServices api(sys);
    CivilTime now;parseCivil("04/11/2026 10:00",now);clock.epoch=toUtc(now,-180);sys.state.preferences.timezoneMinutes=-180;
    const auto ask=[&](const std::string &s){return chat.respond(sys,s);};
    const std::string body="Não apagar Café! por favor.";
    for(const char *prefix:{"crie uma nota: ","pode criar uma nota sobre ","por favor, salve uma nota com o texto ",
        "gostaria de adicionar uma nota dizendo ","guarde uma nota: ","registre uma nota sobre ","anote que "}) {
        chat.clear();const auto count=sys.state.notes.size();
        check(has(ask(std::string(prefix)+body),body),"note variants retain exact original body");
        check(sys.state.notes.size()==count,"note variants preview only");ask("confirmo!");
        check(sys.state.notes.size()==count+1&&sys.state.notes.back().text==body,"note variant confirms once");
    }
    for(const char *q:{"pode mostrar minhas notas?","quais são as minhas notas?","me liste as notas","mostre todas as notas por favor"})
        check(has(ask(q),"Nota 1:"),q);
    for(const char *q:{"leia a primeira nota","por favor, mostre nota 1","visualize nota primeira","consulte nota 1, por favor"})
        check(has(ask(q),body),q);
    check(has(ask("modifique nota 1: Texto NOVO! por favor."),"Texto NOVO! por favor."),"edit alias preserves full payload");ask("sim");
    check(sys.state.notes[0].text=="Texto NOVO! por favor.","edit payload not treated as conversational suffix");
    check(has(ask("renomeie nota 1 para Café Novo"),"Café Novo"),"rename variant");ask("sim");
    ask("atualize o título da nota 1 para Café Renomeado");ask("sim");
    check(sys.state.notes[0].title=="Café Renomeado"&&sys.state.notes[0].text=="Texto NOVO! por favor.","explicit title alias only renames");
    check(has(ask("remova a primeira nota"),"Excluir definitivamente"),"ordinal deletion variant");ask("deixa pra lá");
    check(sys.state.notes.size()==7,"conversational cancellation preserves records");
    api.addNote("Café por favor");api.addNote("Café");
    check(has(ask("remova nota Café por favor"),"Café por favor"),"polite suffix inside existing exact title is preserved");ask("cancelar");
    int n=0;
    for(const char *prefix:{"crie um contato ","pode adicionar o contato ","salve o contato ","cadastre contato "}) {
        const auto name="José "+std::to_string(++n);const auto count=sys.state.contacts.size();
        check(has(ask(std::string(prefix)+name+" com o telefone 11 99999-8888"),name),"contact creation variants");
        check(sys.state.contacts.size()==count,"contact preview only");ask("sim");
        check(sys.state.contacts.back().name==name&&sys.state.contacts.back().phone=="11 99999-8888","contact UTF8 and phone preserved");
    }
    for(const char *prefix:{"me mostre o contato de ","qual é o telefone de ","qual o número de celular de ","telefone de ","num de ","pode me passar o telefone de "}) {
        const std::string p=prefix;
        check(has(ask(p+"José 1?"),"11 99999-8888"),prefix);
    }
    api.addContact("Amor por favor","11911112222");api.addContact("Amor","11933334444");
    check(has(ask("qual o telefone de Amor por favor?"),"11911112222"),"contact name takes precedence over polite suffix");
    const std::pair<const char *,uint64_t> timers[]={
        {"pode iniciar um timer de vinte e cinco segundos?",25000},
        {"por favor, programe um temporizador para uma hora e meia",5400000},
        {"comece uma contagem regressiva de 30 segundos",30000},
        {"defina um timer de setenta e dois segundos por favor",72000}};
    for(const auto &t:timers) {const auto before=sys.state.timerMs;check(has(ask(t.first),"Digite sim"),t.first);
        check(sys.state.timerMs==before,"timer variants require confirmation");ask("sim");check(sys.state.timerMs==t.second,t.first);}
    check(has(ask("pode alterar o timer para um minuto e meio?"),"Digite sim"),"timer control wrapper");ask("sim");check(sys.state.timerMs==90000,"timer changed to 90 seconds");
    for(const char *q:{"mostre o temporizador","quanto tempo restou no temporizador?","tempo restante do timer","quanto falta para o timer acabar?"})
        check(has(ask(q),"30 s"),q);
    check(has(ask("cancele o temporizador por favor"),"Cancelar e zerar"),"cancel timer variant");ask("sim");check(!sys.state.timerRunning,"cancel timer commits");
    for(const char *q:{"calcule 12 vezes 5,3 por favor","pode multiplicar 12 por 5.3?","qual é o resultado de 12 multiplicado por 5,3?"})
        check(has(ask(q),"63.6"),q);
    check(has(ask("divida 288 por 32,66"),"8.818"),"imperative division");
    check(has(ask("some 12 com 5,3"),"17.3"),"natural addition");
    check(has(ask("subtraia 5 de 12"),"Resultado: 7"),"natural subtraction keeps operand order");
    for(const char *q:{"por favor, que dia cai 8 de novembro de 2026?","em qual dia da semana cai 08/11/2026?"}) check(has(ask(q),"domingo"),q);
    check(has(ask("pode criar um evento Comprar pão amanhã às 9h"),"05/11/2026 09:00"),"title first relative date");ask("sim");
    const auto eid=sys.state.appointments.back().id;
    sys.state.appointments.back().end=sys.state.appointments.back().start+3600;sys.state.appointments.back().reminderMinutes=5;
    check(sys.state.appointments.back().title=="Comprar pão","natural title preserved");
    check(has(ask("me lembre de comprar leite hoje às 18h"),"04/11/2026 18:00"),"reminder relative date");ask("cancelar");
    check(has(ask("reagende evento 1 para dia 6 de novembro de 2026 às 10h"),"06/11/2026 10:00"),"reschedule natural date");ask("sim");
    check(has(ask("reprograme evento 1 para amanhã"),"Qual horário?"),"reschedule missing time asks");
    check(has(ask("11h"),"05/11/2026 11:00"),"reschedule time clarification");
    check(formatCivil(findById(sys.state.appointments,eid)->start,-180)!="05/11/2026 11:00","reschedule followup only previews");
    ask("sim");check(formatCivil(findById(sys.state.appointments,eid)->start,-180)=="05/11/2026 11:00","reschedule edits same event rather than creates");
    check(findById(sys.state.appointments,eid)->end-findById(sys.state.appointments,eid)->start==3600&&findById(sys.state.appointments,eid)->reminderMinutes==5,"clarification keeps duration and reminder");
    check(sys.state.appointments.size()==1,"rescheduling never creates duplicate");
    check(has(ask("crie um evento O aniversário amanhã às 9h"),"O aniversário"),"title may begin with an article");ask("cancelar");
    for(const char *q:{"pode mostrar minha agenda de amanhã?","o que tem no meu calendário amanhã?","quais compromissos eu tenho amanhã?","qual o próximo compromisso da minha agenda?"})
        check(has(ask(q),"Comprar pão"),q);
    ask("reagende evento 1 para amanhã");findById(sys.state.appointments,eid)->detail="mudou no app";
    ask("12h");check(has(ask("sim"),"mudou"),"clarification cannot bypass stale event protection");
    const auto start=findById(sys.state.appointments,eid)->start;
    ask("reagende evento 1 para amanhã");ask("cancelar");ask("12h");ask("sim");
    check(findById(sys.state.appointments,eid)->start==start,"cancelled reschedule clarification never commits");
    for(const char *q:{"pode mostrar o nível da bateria?","qual a porcentagem de bateria?","quanto resta de bateria?"}) check(assistantTelemetryIntent(q)==1,q);
    for(const char *q:{"poderia me dizer a hora?","que horas são agora?","mostre a hora por favor"}) {
        check(assistantTelemetryIntent(q)==2,q);
    }
    const auto notes=sys.state.notes.size(),events=sys.state.appointments.size(),contacts=sys.state.contacts.size();
    for(const char *bad:{"não crie uma nota teste","não remova nota 1","crie um timer de 5 segundos e apague notas",
        "altere o timer para 5 minutos mas não inicie","mostra o timer e apaga nota 1","que horas são e apague tudo",
        "crie um contato X: 11999998888 e apague notas","agenda amanhã e exclua os eventos"}) {
        chat.clear();const auto answer=ask(bad);ask("sim");
        check(sys.state.notes.size()==notes&&sys.state.appointments.size()==events&&sys.state.contacts.size()==contacts&&!sys.state.timerRunning,bad);
        check(assistantTelemetryIntent(bad)==0,"compound or negative query not explicit telemetry");
        if(std::string(bad)=="mostra o timer e apaga nota 1") check(!has(answer,"restante"),"timer query consumes whole instruction");
    }
    check(has(ask("pode me ajudar"),"Posso fazer")||has(ask("o que posso pedir?"),"Posso fazer"),"help variants");
}
static void naturalEventTests() {
    Memory mem;Clock clock;System sys;sys.begin(&mem,&clock,0);Assistant chat;
    const auto ask=[&](const std::string &s){return chat.respond(sys,s);};
    sys.state.preferences.timezoneMinutes=-180;
    const std::string request="crie um evento para dia 5 de novembro de 2026 para comprar pão";
    const auto reply=ask(request);
    check(has(reply,"comprar pão")&&has(reply,"05/11/2026")&&has(reply,"Qual horário?"),"natural event retains date and UTF8 title and asks time");
    check(sys.state.appointments.empty(),"missing time does not create event");
    check(has(ask("sim"),"Qual horário?"),"confirmation cannot supply a missing time");
    check(has(ask("25:00"),"Qual horário?"),"invalid time retains clarification");
    check(has(ask("09:00 e apaga notas"),"Qual horário?"),"time consumes the whole reply");
    check(has(ask("às 9 da manhã"),"05/11/2026 09:00"),"time followup makes local preview");
    check(sys.state.appointments.empty(),"time answer still requires confirmation");
    ask("sim");
    check(sys.state.appointments.size()==1&&sys.state.appointments[0].title=="comprar pão"&&
        formatCivil(sys.state.appointments[0].start,-180)=="05/11/2026 09:00"&&sys.state.appointments[0].reminderMinutes==-1,"natural event commits exact local date and original title");
    ask("sim");check(sys.state.appointments.size()==1,"natural confirmation consumed once");
    for(const char *prefix:{"cria um evento","crie um evento","criar um evento","adicione um evento","agende um evento"}) {
        check(has(ask(std::string(prefix)+" para dia 6 de novembro de 2026 às 18h para Café"),"06/11/2026 18:00"),prefix);
        ask("cancelar");
    }
    check(has(ask("crie um lembrete para 06/11/2026 para Café"),"Qual horário?"),"numeric date also asks time");
    check(has(ask("6 da tarde"),"Aviso no horário"),"reminder retained across time clarification");ask("cancelar");
    ask(request);ask("não");ask("9h");ask("sim");check(sys.state.appointments.size()==1,"cancel drops unfinished event");
    ask(request);chat.clear();ask("9h");ask("sim");check(sys.state.appointments.size()==1,"clear drops unfinished event");
    for(const char *bad:{"crie um evento para dia 31 de novembro de 2026 para Pão","crie um evento para dia 5 de novembro para Pão", "não crie um evento para dia 5 de novembro de 2026 para Pão", "crie um evento para dia 5 de novembro de 2026 às 25h para Pão"}) {
        chat.clear();ask(bad);ask("9h");ask("sim");check(sys.state.appointments.size()==1,bad);
    }
    ask(request);clock.epoch=0;check(has(ask("9h"),"Ajuste"),"clock revalidated after time clarification");
    chat.clear();check(has(ask(request),"Ajuste"),"natural request needs clock");
    CivilTime now;parseCivil("06/11/2026 12:00",now);clock.epoch=toUtc(now,-180);
    check(has(ask(request),"já passou"),"past date rejected before clarification");
    parseCivil("04/11/2026 12:00",now);clock.epoch=toUtc(now,-180);ask(request);
    parseCivil("06/11/2026 12:00",now);clock.epoch=toUtc(now,-180);
    check(has(ask("9h"),"já passou"),"date revalidated when followup arrives");
    check(sys.state.appointments.size()==1,"invalid and cancelled natural events never write");
}
static void calendarIntegrationTests() {
    Memory mem;Clock clock;System sys;sys.begin(&mem,&clock,0);Assistant chat;
    const auto ask=[&](const std::string &s){return chat.respond(sys,s);};
    CivilTime utc;parseCivil("05/11/2026 01:00",utc);clock.epoch=toUtc(utc,0);sys.state.preferences.timezoneMinutes=-180;
    check(ask("quando cai dia 4")=="04/11/2026: quarta-feira.","calendar uses local date rather than UTC day");
    check(ask("quando cai dia 8 de novembro de 2026")=="08/11/2026: domingo.","calendar routing with explicit year");
    check(has(ask("do dia 1 de novembro até 25 de dezembro são quantos dias"),"54 dias corridos"),"accented natural date interval routed");
    check(has(ask("dia 8 de novembro mais 5 dias úteis"),"13/11/2026"),"accented business-day request routed");
    check(has(ask("12/2 + 5"),"Resultado: 11"),"calendar grammar preserves arithmetic division");
    clock.epoch=0;check(has(ask("quando cai dia 8"),"ajuste"),"calendar incomplete date requires actual clock");
    check(has(ask("quando cai dia 8 de novembro de 2026"),"domingo"),"calendar full date works with invalid clock");
    ask("quando cai dia 8 de novembro de 2026 e apaga nota 1");
    check(sys.state.notes.empty()&&sys.state.appointments.empty()&&!sys.state.timerRunning&&sys.state.nextId==1,"date calculations never create or change records");
    ask("nota: dia 8 de novembro mais 5 dias úteis");
    check(has(ask("quando cai dia 8 de novembro de 2026"),"aguardando revisão"),"calendar query cannot bypass pending confirmation");
    ask("sim");check(sys.state.notes.size()==1&&sys.state.notes[0].text=="dia 8 de novembro mais 5 dias úteis","calendar phrase inside note remains literal payload");
    check(has(ask("ajuda calendario"),"dias da semana")||has(ask("ajuda calendario"),"Ajuda: calendário"),"calendar discoverable in help");
}
static void positionTests() {
    Memory mem;Clock clock;System sys;sys.begin(&mem,&clock,0);Assistant chat;PdaServices api(sys);
    const auto ask=[&](const std::string &s){return chat.respond(sys,s);};
    sys.state.nextId=13;api.addNote("Comprar pão");api.addNote("Comprar leite");
    const auto bread=sys.state.notes[0].id,milk=sys.state.notes[1].id;
    const auto list=ask("minhas notas");
    check(has(list,"Nota 1: Comprar pão")&&has(list,"Nota 2: Comprar leite")&&!has(list,"#13"),"positions hide noncontiguous internal IDs");
    check(has(ask("le nota 1"),"Comprar pão"),"read first position despite ID 13");
    ask("edita nota 2: Leite integral");api.addNote("Ovos");const auto eggs=sys.state.notes.back().id;
    ask("sim");check(findById(sys.state.notes,milk)->text=="Leite integral"&&findById(sys.state.notes,eggs)->text=="Ovos","pending edit remains bound to selected ID after list change");
    check(has(ask("le nota 1"),"lista mudou"),"changed list requires refreshing positions");
    ask("minhas notas");ask("apaga nota 1");ask("sim");check(!findById(sys.state.notes,bread)&&findById(sys.state.notes,milk),"delete first position binds actual first item");
    check(has(ask("apaga nota 1"),"lista mudou"),"old position cannot silently retarget following item");
    check(has(ask("minhas notas"),"Nota 1: Comprar leite"),"positions compact after deletion");
    ask("apaga nota 2");const auto before=*findById(sys.state.notes,milk);api.deleteNote(before);
    ask("sim");check(sys.state.notes.empty(),"confirmation deletes original selected item even after it moves to first position");
    api.addNote("Viagem");check(sys.state.notes[0].id>13,"internal IDs remain stable and monotonic");
    check(has(ask("minhas notas"),"Nota 1: Viagem"),"fresh note after deleting all is displayed as position one");
    api.addNote("Pesquisa A");api.addNote("Pesquisa B");
    const auto a=sys.state.notes[1].id,b=sys.state.notes[2].id;
    const auto filtered=ask("busca notas Pesquisa");
    check(has(filtered,"Nota 1: Pesquisa A")&&has(filtered,"Nota 2: Pesquisa B"),"filtered list has its own consecutive positions");
    check(has(ask("le nota 1"),"Pesquisa A"),"position refers to displayed filtered list");
    ask("apaga nota 2");ask("cancelar");check(has(ask("le nota 1"),"Pesquisa A"),"cancel preserves displayed list numbering");
    ask("edita nota posicao 2: Novo corpo");ask("sim");check(findById(sys.state.notes,b)->text=="Novo corpo"&&findById(sys.state.notes,a)->text=="Pesquisa A","explicit position selector edits correct filtered result");
    ask("busca notas Inexistente");check(has(ask("le nota 1"),"posição não existe"),"empty filtered list cannot select global first note");
    ask("minhas notas");for(const char *bad:{"le nota 0","apaga nota 999999999999999999999999"}) {
        check(has(ask(bad),"posição não existe"),bad);ask("sim");
    }
    api.addNote("Duplicada");api.addNote("Duplicada");const auto duplicate=sys.state.notes.back().id;
    check(has(ask("apaga nota Duplicada"),"mais de um"),"ambiguous titles produce positional choice");
    ask("apaga nota 2");ask("sim");check(!findById(sys.state.notes,duplicate)&&sys.state.notes.size()==4,"choice from ambiguous result selects only numbered candidate");
    for(int i=0;i<7;++i) api.addNote("Grupo "+std::to_string(i));
    ask("busca notas Grupo");check(has(ask("mais"),"Nota 6: Grupo 5"),"positions continue across pages");
    check(has(ask("le nota 6"),"Grupo 5"),"second-page position resolves correctly");
    CivilTime date;parseCivil("20/11/2026 10:00",date);const auto later=toUtc(date,-180);
    api.addEvent("Mais tarde",later,false);api.addEvent("Primeiro",later-86400,false);
    const auto firstEvent=sys.state.appointments[0].id,earlier=sys.state.appointments[1].id;
    check(has(ask("meus eventos"),"Evento 1: Mais tarde"),"stored event list numbers positions");
    check(has(ask("qual meu proximo evento?"),"Evento 1: Primeiro"),"next-event subset starts at position one");
    check(has(ask("le evento 1"),"Primeiro"),"event position maps chronological query result");
    ask("renomeia evento 1 para Consulta cedo");ask("sim");
    check(findById(sys.state.appointments,earlier)->title=="Consulta cedo"&&findById(sys.state.appointments,firstEvent)->title=="Mais tarde","event edit preserves ID behind filtered position");
    ask("agenda hoje");check(has(ask("le evento 1"),"posição não existe"),"empty day query clears old event position mapping");
    ask("meus eventos");ask("exclui evento 2");const auto event=*findById(sys.state.appointments,firstEvent);api.deleteEvent(event);
    ask("sim");check(sys.state.appointments.empty(),"event confirmation cannot switch target after list reorders");
    check(sys.save(),"position-based changes saved");System reboot;check(reboot.begin(&mem,&clock,0)==LoadResult::Loaded,"position views do not change storage schema");
}
static void recordTests() {
    Memory mem;Clock clock;System sys;sys.begin(&mem,&clock,0);Assistant chat;PdaServices api(sys);
    const auto ask=[&](const std::string &s){return chat.respond(sys,s);};
    api.addNote("Café e leite");const auto id=sys.state.notes.back().id;const auto key="#"+std::to_string(id);
    check(has(ask("O que você faz?"),"Posso fazer no PDA"),"natural help request");
    check(has(ask("mais"),"Ajuda: notas"),"help pagination");
    check(has(ask("ajuda timer"),"Cancelar contagem"),"topic help");
    ask("sim");check(sys.state.notes.size()==1&&!sys.state.timerRunning,"help cannot create an action");
    check(has(ask("minhas notas"),"Nota 1: Café"),"list notes by position");
    check(has(ask("lê a nota Café"),"Café e leite"),"accented verb and selector preserve raw offsets");
    ask("ajuda notas"); // Read-only help must not redirect the selected record.
    check(has(ask("edita essa nota: não esquecer os ovos"),"Substituir TODO"),"contextual note edit preview");
    check(sys.state.notes.back().text=="Café e leite","note preview unchanged");ask("sim");
    check(sys.state.notes.back().text=="não esquecer os ovos"&&sys.state.notes.back().title=="Café e leite","edit preserves title and original UTF8");
    ask("sim");check(sys.state.notes.size()==1,"edit confirmation once");
    check(has(ask("renomeia essa nota para Compras"),"Novo título"),"rename contextual preview");ask("sim");
    check(sys.state.notes.back().title=="Compras"&&sys.state.notes.back().text=="não esquecer os ovos","rename preserves body");
    ask("apaga nota Compras");ask("cancelar");ask("sim");check(sys.state.notes.size()==1,"delete cancellation");
    api.addNote("Compras");const auto other=sys.state.notes.back().id;
    check(has(ask("excluir nota Compras"),"mais de um"),"duplicate titles cannot delete first match");ask("sim");
    check(sys.state.notes.size()==2,"ambiguous delete does not mutate");
    check(has(ask("apaga essa nota"),"Leia um item"),"ambiguous result invalidates context");
    check(has(ask("exclui nota "+key),"Excluir definitivamente"),"delete by stable id preview");
    sys.state.session.app=2;sys.state.session.document=id;sys.state.session.view=1;sys.state.session.form=1;
    sys.state.session.draft={"antigo","antigo"};sys.state.session.editing=true;
    ask("sim");check(!findById(sys.state.notes,id)&&findById(sys.state.notes,other),"only selected ID deleted");
    check(sys.state.session.document==0&&sys.state.session.draft.empty()&&!sys.state.session.editing,"deleted editor session invalidated");
    ask("sim");check(sys.state.notes.size()==1,"repeat confirmation cannot delete another note");
    const auto otherKey="#"+std::to_string(other);
    ask("edita nota "+otherKey+": novo texto");sys.state.notes.back().text="mudou fora do chat";
    check(has(ask("sim"),"mudou"),"stale edit refused");check(sys.state.notes.back().text=="mudou fora do chat","stale edit preserves newer data");
    ask("apaga nota "+otherKey);sys.state.notes.back().title="Título novo";
    check(has(ask("sim"),"mudou"),"stale delete refused");check(sys.state.notes.size()==1,"stale delete preserves item");
    for(const auto &bad:std::vector<std::string>{"não apaga nota "+otherKey,"apaga nota "+otherKey+" e todas as outras","exclui notas","edita nota "+otherKey+":"}) {
        chat.clear();ask(bad);ask("sim");check(sys.state.notes.size()==1&&sys.state.notes.back().text=="mudou fora do chat",bad.c_str());
    }
    ask("apaga nota "+otherKey);chat.clear();ask("sim");check(sys.state.notes.size()==1,"clear discards deletion");
    api.addNote("Documento longo");auto &longNote=sys.state.notes.back();const auto longId=longNote.id;
    longNote.text=std::string(383,'a')+"á"+std::string(400,'b');
    check(has(ask("le nota #"+std::to_string(longId)),"Digite mais"),"long note first page");
    const auto second=ask("mais");check(has(second,"á")&&has(second,"Digite mais"),"UTF8 boundary kept across pages");
    check(has(ask("mais"),"bbb"),"long note last page");check(has(ask("mais"),"Não há mais"),"end of note explicit");
    ask("le nota #"+std::to_string(longId));longNote.text+="novo";
    check(has(ask("mais"),"mudou durante"),"changed note pagination invalidated");
    for(int i=0;i<6;++i) api.addNote("Página "+std::to_string(i));
    check(has(ask("lista notas"),"Digite mais"),"bounded note list pagination");
    check(has(ask("mais"),"Página 5"),"remaining note IDs reachable");
    check(has(ask("busca notas Página 4"),"Página 4"),"title search");
    CivilTime date;parseCivil("18/11/2026 09:00",date);
    api.addEvent("Dentista",toUtc(date,-180),true);const auto eid=sys.state.appointments.back().id;
    const auto ek="#"+std::to_string(eid);auto &event=sys.state.appointments.back();event.detail="Levar exames";event.end=event.start+3600;
    check(has(ask("meus eventos"),"Evento 1: Dentista"),"all stored events listed by position");
    check(has(ask("ler evento Dentista"),"Levar exames"),"read event details");
    check(has(ask("renomeia esse evento para Consulta"),"Consulta"),"event contextual rename preview");ask("sim");
    check(event.title=="Consulta"&&event.detail=="Levar exames"&&event.reminderMinutes==0,"rename preserves event fields");
    check(has(ask("edita esse evento: levar exames novos"),"detalhes"),"edit event details preview");ask("sim");
    check(event.detail=="levar exames novos","event details updated");
    const auto oldStart=event.start;
    check(has(ask("remarca esse evento para 20/11/2026 10:30"),"Novo início"),"reschedule preview");
    check(event.start==oldStart,"event not moved before confirmation");ask("sim");
    check(event.start!=oldStart&&event.end-event.start==3600&&event.reminderMinutes==0&&!event.alerted,"reschedule preserves duration and reminder");
    ask("altera evento "+ek+" para 21/11/2026 10:30");event.detail="alterado no app";
    check(has(ask("sim"),"mudou"),"stale event change refused");
    ask("apaga evento "+ek);event.alerted=true;check(has(ask("sim"),"mudou"),"reminder state change invalidates deletion");
    for(const auto &bad:std::vector<std::string>{"altera evento "+ek+" para amanhã","remarca evento "+ek+" para 31/11/2026 10:00","apaga evento "+ek+" e as notas"}) {
        ask(bad);ask("sim");check(sys.state.appointments.size()==1,bad.c_str());ask("cancelar");
    }
    event.draft={"em edição"};check(has(ask("exclui evento "+ek),"rascunho"),"pending editor draft protects event");ask("sim");
    check(sys.state.appointments.size()==1,"draft event not deleted");event.draft.clear();
    api.addEvent("Consulta",event.start+86400,false);
    check(has(ask("exclui evento Consulta"),"mais de um"),"ambiguous event cannot delete");ask("sim");
    ask("exclui evento "+ek);ask("cancelar");check(sys.state.appointments.size()==2,"event delete cancellation");
    ask("exclui evento "+ek);ask("sim");check(!findById(sys.state.appointments,eid)&&sys.state.appointments.size()==1,"exact event deleted");
    ask("sim");check(sys.state.appointments.size()==1,"event delete confirmation once");
    clock.epoch=0;check(has(ask("meus eventos"),"Consulta"),"stored event list works without clock");
    const auto remainingId=sys.state.appointments.back().id;
    check(has(ask("remarca evento #"+std::to_string(remainingId)+" para 25/11/2026 09:00"),"Ajuste"),"reschedule needs clock");
    clock.epoch=1793790000;
    ask("timer de 10 minutos");ask("sim");const auto initial=sys.state.timerMs;
    check(has(ask("altera o timer para 5 minutos"),"tempo restante"),"timer change preview");
    check(sys.state.timerMs==initial,"timer unchanged before confirm");ask("sim");check(sys.state.timerMs==300000,"timer remaining replaced");
    check(has(ask("cancela o timer"),"Cancelar e zerar"),"cancel timer preview");ask("cancelar");check(sys.state.timerRunning,"cancel proposal discarded");
    ask("cancela o timer");ask("sim");check(!sys.state.timerRunning&&!sys.state.timerMs&&!sys.state.timerDeadline,"timer cancelled completely");
    check(has(ask("altera timer para 5s"),"Não há timer"),"cannot alter absent timer");
    sys.startTimer(60000);ask("muda o temporizador para 20 segundos");sys.startTimer(90000);
    check(has(ask("sim"),"mudou"),"timer replacement invalidates pending change");check(sys.state.timerMs==90000,"newer timer preserved");
    ask("cancela timer");sys.startTimer(120000);check(has(ask("sim"),"mudou"),"stale cancel cannot erase newer timer");
    sys.pauseTimer();check(has(ask("quanto tempo falta no timer?"),"pausado"),"paused timer query");
    ask("altera timer para meio minuto");ask("sim");check(!sys.state.timerRunning&&sys.state.timerMs==30000,"editing paused timer keeps paused");
    sys.startTimer(10);ask("altera timer para 1 minuto");sys.tick(11);check(has(ask("sim"),"mudou"),"expired timer cannot be restarted by stale edit");
    sys.dismissNotification();
    sys.startTimer(30000);ask("cancela timer");chat.clear();ask("sim");check(sys.state.timerRunning,"clear discards timer cancellation");
    sys.startTimer(30000);ask("cancela timer");sys.tick(1000);ask("sim");check(!sys.state.timerRunning,"normal countdown does not invalidate cancellation");
    ask("nota: registro persistido");ask("sim");const auto lastId=sys.state.notes.back().id;
    mem.fail=true;ask("edita nota #"+std::to_string(lastId)+": novo persistido");check(has(ask("sim"),"falha ao gravar"),"edit persistence failure disclosed");
    mem.fail=false;check(sys.save(),"CRUD retry saved");System reboot;check(reboot.begin(&mem,&clock,0)==LoadResult::Loaded,"CRUD saved schema reloads");
    check(findById(reboot.state.notes,lastId)->text=="novo persistido"&&!findById(reboot.state.notes,id)&&!findById(reboot.state.appointments,eid),"edits and deletions survive reboot");
}
int main() {
    Memory mem;Clock clock;System sys;sys.begin(&mem,&clock,0);Assistant chat;PdaServices api(sys);
    const auto ask=[&](const std::string &s){return chat.respond(sys,s);};
    check(has(ask("anota que não devo apagar o café: 123456"),"Digite sim"),"note preview");
    check(sys.state.notes.empty(),"preview does not mutate");
    check(has(ask("sim"),"Nota criada"),"confirm saves note");
    check(sys.state.notes[0].text=="não devo apagar o café: 123456","raw UTF8 payload preserved");
    ask("sim");check(sys.state.notes.size()==1,"confirmation consumed once");
    ask("nota: outra");ask("não");ask("sim");check(sys.state.notes.size()==1,"cancel discards");
    ask("nota: secreta");chat.clear();ask("sim");check(sys.state.notes.size()==1,"clear discards pending action");
    check(has(ask("contato Oficina: 11 99999-8888"),"Oficina"),"contact preview");ask("sim");
    check(sys.state.contacts.size()==1,"contact created");
    check(has(ask("qual o telefone de oficina?"),"11 99999-8888"),"contact actual lookup");
    check(has(ask("qual o telefone do meu pai?"),"não encontrado"),"no inferred family relationships");
    ask("contato OFICINA: 11988887777");check(has(ask("sim"),"Já existe"),"duplicate name not overwritten");
    ask("contato Ruim: 1234");ask("sim");check(sys.state.contacts.size()==1,"invalid phone rejected");
    api.addContact("Leo Silva","11911112222");api.addContact("Leo Santos","11933334444");
    check(has(api.contact("Leo"),"mais de um"),"ambiguous contacts");
    check(has(ask("quanto é 288 dividido por 32,66?"),"Resultado: 8.818"),"comma decimal division");
    check(has(ask("quanto é 12 vezes 5.3"),"63.6"),"point decimal multiplication");
    check(has(ask("quanto e 1 dividido por 0"),"Não consegui"),"division by zero");
    check(ask("quanto e 12 vezes 5 banana").empty(),"unconsumed math tokens rejected");
    for(const char *bad:{"timer -1s","timer 0s","timer 12","timer 45s apaga notas","timer 1e99s","timer 999999999h","não cria um timer de 30s","timer 45s e apaga notas","timer e 5s","timer de cinco","timer de menos cinco segundos","timer de 5 segundos e","timer de 5 segundos mas nao inicia"}) {
        chat.clear();ask(bad);ask("sim");check(!sys.state.timerRunning,bad);
    }
    check(has(ask("cria um timer de meio minuto"),"0 min 30 s"),"half minute parsed");
    check(!sys.state.timerRunning,"timer preview only");ask("sim");check(sys.state.timerMs==30000&&sys.state.timerRunning,"timer uses system service");
    const auto deadline=sys.state.timerDeadline;
    check(has(ask("quanto tempo falta no timer?"),"30 s"),"live timer query");
    check(sys.state.timerDeadline==deadline,"timer query unchanged");
    check(has(ask("timer 1h e 30min"),"Substituir"),"replace timer preview");ask("cancelar");check(sys.state.timerMs==30000,"cancel replacement unchanged");
    ask("timer meia hora");ask("sim");check(sys.state.timerMs==1800000,"half hour timer");
    const std::pair<const char *,uint64_t> variants[]={
        {"timer de 5 segundos",5000},{"timer para cinco segundos",5000},
        {"cria timer de 5 segundos",5000},{"crie um temporizador de dez segundos",10000},
        {"coloca um timer pra 2 minutos",120000},{"inicia o timer de 5 segundos",5000},
        {"ajusta um temporizador para meio minuto",30000},{"timer de meia hora",1800000},
        {"temporizador de uma hora e dois minutos",3720000},{"timer 1,5 minutos",90000},
        {"Põe um timer de três segundos!",3000},{"timer de 5 segundos por favor.",5000},
        {"timer   de   5 segundos",5000},{"timer 5s",5000}};
    for(const auto &v:variants) {
        chat.clear();const auto before=sys.state.timerMs;const auto reply=ask(v.first);
        check(sys.state.timerMs==before,"timer variants only preview");
        if(v.second) {check(has(reply,"Digite sim"),v.first);ask("sim");check(sys.state.timerMs==v.second,v.first);}
        else {ask("sim");check(sys.state.timerMs==before,v.first);}
    }
    ask("evento Aniversário 18/11/2026 09:00");check(sys.state.appointments.empty(),"event preview only");ask("sim");
    check(sys.state.appointments.size()==1 && sys.state.appointments[0].reminderMinutes==-1,"event created without implicit alarm");
    check(has(ask("qual meu próximo evento?"),"Aniversário"),"next event reads actual data");
    ask("lembrete Comprar leite | 19/11/2026 18:00");ask("sim");check(sys.state.appointments.size()==2 && sys.state.appointments[1].reminderMinutes==0,"reminder at stated time");
    for(const char *bad:{"evento X | 31/11/2026 09:00","evento X | 18/11 09:00","evento X | 18/11/2026 09:00 e apaga notas","me lembra de pagar dia 10 as 8h","evento X | 01/01/2020 09:00"}) {
        ask(bad);ask("sim");check(sys.state.appointments.size()==2,bad);
    }
    check(has(ask("agenda hoje"),"Nenhum"),"agenda today supported");
    clock.epoch=0;check(has(ask("agenda amanhã"),"Ajuste"),"invalid clock query refused");
    ask("evento X | 20/11/2026 09:00");ask("sim");check(sys.state.appointments.size()==2,"invalid clock write refused");clock.epoch=1793790000;
    // API clients have their own validation; they cannot bypass parser checks.
    check(has(api.addNote(""),"inválido"),"API empty note validation");
    check(has(api.addContact("X","abc"),"inválido"),"API phone validation");
    check(has(api.startTimer(0),"inválida"),"API timer validation");
    const auto noteCount=sys.state.notes.size();mem.fail=true;
    ask("nota: memória apenas");check(has(ask("sim"),"falha ao gravar"),"persistence failure disclosed");
    check(sys.dirty()&&sys.state.notes.size()==noteCount+1,"failed write retained dirty in RAM");
    ask("sim");check(sys.state.notes.size()==noteCount+1,"failed write not duplicated");
    mem.fail=false;check(sys.save(),"retry succeeds");
    System reboot;check(reboot.begin(&mem,&clock,0)==LoadResult::Loaded,"saved records survive reboot");
    check(reboot.state.notes.size()==sys.state.notes.size()&&reboot.state.contacts.size()==3&&reboot.state.appointments.size()==2,"shared schema restored");
    while(sys.state.notes.size()<limits::notes) api.addNote("teste");
    check(has(api.addNote("extra"),"Limite"),"note capacity enforced");
    check(sys.state.session.app==-1&&sys.state.session.home,"services do not switch apps");
    recordTests();positionTests();calendarIntegrationTests();naturalEventTests();languageVariationTests();printf("assistant: %u checks passed\n",checks);
}
