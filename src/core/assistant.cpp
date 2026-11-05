#include "pda/assistant.h"
#include "pda/datetime.h"
#include "pda/expression.h"
#include "pda/catalog.h"
#include "pda/calendar_math.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace pda {
namespace {
std::string trim(const std::string &s) {
    const auto a=s.find_first_not_of(" \r\n\t");
    return a==std::string::npos?"":s.substr(a,s.find_last_not_of(" \r\n\t")-a+1);
}
// Fold Portuguese accents without changing the original argument payload.
std::string fold(const std::string &s) {
    std::string out;
    for (size_t i=0;i<s.size();++i) {
        unsigned char c=s[i];
        if (c==0xc3 && i+1<s.size()) {
            const unsigned char d=s[++i];
            if ((d>=0x80 && d<=0x85)||(d>=0xa0 && d<=0xa5)) out+='a';
            else if (d==0x87 || d==0xa7) out+='c';
            else if ((d>=0x88 && d<=0x8b)||(d>=0xa8 && d<=0xab)) out+='e';
            else if ((d>=0x8c && d<=0x8f)||(d>=0xac && d<=0xaf)) out+='i';
            else if ((d>=0x92 && d<=0x96)||(d>=0xb2 && d<=0xb6)) out+='o';
            else if ((d>=0x99 && d<=0x9c)||(d>=0xb9 && d<=0xbc)) out+='u';
            else out+='?';
        } else out+=c>='A'&&c<='Z'?char(c+32):char(c);
    }
    return trim(out);
}
bool starts(const std::string &s,const char *p) { return s.compare(0,std::char_traits<char>::length(p),p)==0; }
bool word(const std::string &s,const char *w) { return (" "+s+" ").find(std::string(" ")+w+" ")!=std::string::npos; }
size_t rawOffset(const std::string &raw,size_t normalizedBytes) {
    size_t i=0;
    while(i<raw.size()&&normalizedBytes--) i+=(uint8_t(raw[i])==0xc3&&i+1<raw.size())?2:1;
    return i;
}
bool prefixPayload(const std::string &raw,const std::string &norm,const char *prefix,std::string &payload) {
    if (!starts(norm,prefix)) return false;
    payload=trim(raw.substr(rawOffset(raw,std::char_traits<char>::length(prefix))));
    return true;
}
std::string commandKey(std::string s,bool polite=true) {
    s=fold(s);
    while(!s.empty()&&(s.back()=='?'||s.back()=='.'||s.back()=='!')) s=trim(s.substr(0,s.size()-1));
    if(!polite) return s;
    for(const char *suffix:{", por favor"," por favor"," pra mim"," para mim"}) {
        const size_t n=std::char_traits<char>::length(suffix);
        if(s.size()>n&&s.compare(s.size()-n,n,suffix)==0) s=trim(s.substr(0,s.size()-n));
    }
    return s;
}
// Rewrite command heads only. Never fold or rewrite names and document bodies.
std::string commandText(std::string raw) {
    raw=trim(raw);
    for(int pass=0;pass<3;++pass) {
        bool matched=false;
        for(const char *p:{"por favor, ","por favor ","voce pode ","vc pode ","pode ","poderia ","quero que voce ","gostaria de ","quero "}) {
            std::string rest;if(prefixPayload(raw,fold(raw),p,rest)) {raw=rest;matched=true;break;}
        }
        if(!matched) break;
    }
    const std::pair<const char *,const char *> heads[]={
        {"me mostre ","mostra "},{"me mostra ","mostra "},{"me liste ","lista "},
        {"me passar ","me passa "},{"me dizer ","me diga "},{"mostrar ","mostra "},
        {"me lembre de ","me lembra de "},{"lembre-me de ","me lembra de "},{"lembra de ","me lembra de "},
        {"me avise para ","me lembra de "},{"me avisa para ","me lembra de "},
        {"criar ","cria "},{"crie ","cria "},{"adicionar ","adiciona "},{"adicione ","adiciona "},{"inclua ","adiciona "},{"incluir ","adiciona "},
        {"cadastre ","cria "},{"cadastrar ","cria "},{"registre ","cria "},{"registrar ","cria "},
        {"salvar ","salva "},{"salve ","salva "},{"guardar ","salva "},{"guarde ","salva "},
        {"anotar ","anota "},{"anote ","anota "},{"mostre ","mostra "},{"exiba ","mostra "},{"visualize ","mostra "},{"consulte ","mostra "},
        {"pesquise ","busca "},{"pesquisar ","busca "},{"encontre ","busca "},{"encontrar ","busca "},
        {"delete ","apaga "},{"remova ","apaga "},{"elimine ","apaga "},{"eliminar ","apaga "},
        {"modifique ","altera "},{"modificar ","altera "},{"atualize ","altera "},{"atualizar ","altera "},{"mude ","altera "},
        {"reagende ","remarca "},{"reagendar ","remarca "},{"remarque ","remarca "},
        {"reprograma ","remarca "},{"reprograme ","remarca "},
        {"iniciar ","inicia "},{"inicie ","inicia "},{"comece ","inicia "},{"comecar ","inicia "},
        {"coloque ","coloca "},{"colocar ","coloca "},{"defina ","define "},{"definir ","define "},
        {"ajuste ","ajusta "},{"ajustar ","ajusta "},{"programe ","define "},{"programar ","define "}};
    for(const auto &h:heads) {
        std::string rest;if(prefixPayload(raw,fold(raw),h.first,rest)) {raw=std::string(h.second)+rest;break;}
    }
    for(const auto &h:{std::make_pair("altera o titulo da nota ","renomeia nota "),
        std::make_pair("altera o titulo do evento ","renomeia evento "),
        std::make_pair("altera o texto da nota ","edita nota "),
        std::make_pair("altera os detalhes do evento ","edita evento "),
        std::make_pair("altera a data do evento ","remarca evento "),
        std::make_pair("altera o horario do evento ","remarca evento "),
        std::make_pair("altera o tempo do timer ","altera o timer "),
        std::make_pair("altera a duracao do timer ","altera o timer "),
        std::make_pair("altera a duracao do temporizador ","altera o temporizador ")}) {
        std::string rest;if(prefixPayload(raw,fold(raw),h.first,rest)) {raw=std::string(h.second)+rest;break;}
    }
    const auto key=commandKey(raw);
    for(const char *q:{"minhas notas","minhas anotacoes","quais sao as minhas notas","quais sao minhas notas","que notas tenho","quais notas tenho","mostra todas as notas","lista todas as notas","lista as minhas notas"})
        if(key==q) return "minhas notas";
    for(const char *q:{"quais sao os meus eventos","quais eventos tenho","mostra todos os eventos","lista todos os eventos","lista os meus eventos","meus compromissos"})
        if(key==q) return "meus eventos";
    for(const char *q:{"o que voce sabe fazer","me ajuda","mostra os comandos","lista os comandos","quais sao os comandos","o que posso pedir"})
        if(key==q) return "ajuda";
    if(key=="continue"||key=="seguir"||key=="proxima"||key=="pode continuar") return "mais";
    return raw;
}
bool noteRequest(const std::string &raw,std::string &payload) {
    auto norm=fold(raw);
    for(const char *p:{"anota ai que ","anota que ","anota ","nota: "})
        if(prefixPayload(raw,norm,p,payload)) return true;
    auto head=raw;
    for(const char *verb:{"cria ","adiciona ","salva ","faz ","faca "}) {
        std::string rest;if(!prefixPayload(raw,norm,verb,rest)) continue;
        head=rest;const auto key=fold(head);
        for(const char *noun:{"uma nota rapidinho: ","uma nota: ","uma nota ","nota: ","nota ","uma anotacao: ","uma anotacao ","anotacao: ","anotacao "}) {
            if(!prefixPayload(head,key,noun,payload)) continue;
            for(const char *connector:{"que ","sobre ","dizendo ","com o texto "}) {
                std::string body;if(prefixPayload(payload,fold(payload),connector,body)) {payload=body;break;}
            }
            return true;
        }
    }
    return false;
}
bool digits(const std::string &s,int &value) {
    if(s.empty()||s.size()>4||s.find_first_not_of("0123456789")!=std::string::npos) return false;
    value=atoi(s.c_str());return true;
}
bool eventTime(std::string s,CivilTime &date) {
    s=fold(s);
    if(starts(s,"as ")) s=trim(s.substr(3));
    if(!s.empty()&&(s.back()=='.'||s.back()=='!')) s.pop_back();
    int period=0;
    for(const auto &p:{std::make_pair(" da manha",1),std::make_pair(" da tarde",2),std::make_pair(" da noite",2)}) {
        const std::string suffix=p.first;
        if(s.size()>suffix.size()&&s.compare(s.size()-suffix.size(),suffix.size(),suffix)==0) {
            period=p.second;s=trim(s.substr(0,s.size()-suffix.size()));break;
        }
    }
    if(!s.empty()&&s.back()=='h') s.pop_back();
    const auto colon=s.find(':');int h=0,m=0;
    if(!digits(s.substr(0,colon),h)||(colon!=std::string::npos&&!digits(s.substr(colon+1),m))) return false;
    if(h>23||m>59||(period&&(h<1||h>12))) return false;
    if(period==1&&h==12) h=0;
    if(period==2&&h<12) h+=12;
    date.hour=h;date.minute=m;date.second=0;return true;
}
bool eventRequest(const std::string &raw,const std::string &norm,std::string &payload,bool &reminder) {
    if(prefixPayload(raw,norm,"me lembra de ",payload)) {reminder=true;return true;}
    auto s=norm;size_t consumed=0;
    for(const char *v:{"cria ","crie ","criar ","adiciona ","adicione ","agenda ","agende ","define ","marca ","marque ","marcar ","salva "}) {
        if(starts(s,v)) {consumed+=std::char_traits<char>::length(v);s=s.substr(std::char_traits<char>::length(v));break;}
    }
    for(const char *a:{"um ","o "}) if(starts(s,a)) {consumed+=std::char_traits<char>::length(a);s=s.substr(std::char_traits<char>::length(a));break;}
    for(const char *noun:{"evento ","compromisso ","lembrete "}) if(starts(s,noun)) {
        reminder=starts(noun,"lembrete");consumed+=std::char_traits<char>::length(noun);
        payload=trim(raw.substr(rawOffset(raw,consumed)));return true;
    }
    return false;
}
bool eventDetails(const std::string &payload,CivilTime &date,std::string &title,bool &hasTime,bool clockValid=false,CivilTime today=CivilTime(),bool reordered=false) {
    hasTime=true;
    if(payload.size()>=17&&parseCivil(payload.substr(payload.size()-16),date)) {
        title=trim(payload.substr(0,payload.size()-16));
        if(!title.empty()&&(title.back()=='|'||title.back()==':')) title=trim(title.substr(0,title.size()-1));
        return !title.empty();
    }
    // Also accept title first: "Comprar pão amanhã às 9h".
    const auto normalized=fold(payload);
    const auto first=normalized.substr(0,normalized.find(' '));
    const bool dateFirst=first=="para"||first=="pra"||starts(normalized,"o dia ")||first=="dia"||first=="hoje"||first=="amanha"||
        (!first.empty()&&first[0]>='0'&&first[0]<='9');
    if(!reordered&&!dateFirst) {
        for(const char *separator:{" para dia "," no dia "," dia "," em "," amanha"," hoje"}) {
            const auto at=normalized.find(separator);if(at==std::string::npos||!at) continue;
            const bool relative=std::string(separator)==" amanha"||std::string(separator)==" hoje";
            const auto dateAt=at+(relative?1:std::char_traits<char>::length(separator));
            const auto rearranged=trim(payload.substr(rawOffset(payload,dateAt)))+" para "+trim(payload.substr(0,rawOffset(payload,at)));
            if(eventDetails(rearranged,date,title,hasTime,clockValid,today,true)) return true;
        }
    }
    // Date first, title after "para"; retain the original UTF-8 title.
    const auto s=fold(payload);std::vector<std::string> tokens;std::vector<size_t> ends;
    for(size_t i=0;i<s.size();) {
        const auto a=s.find_first_not_of(" \t",i);if(a==std::string::npos) break;
        const auto b=s.find_first_of(" \t",a);tokens.push_back(s.substr(a,b-a));
        ends.push_back(b==std::string::npos?s.size():b);i=ends.back();
    }
    size_t i=0;
    if(i<tokens.size()&&(tokens[i]=="para"||tokens[i]=="pra")) ++i;
    if(i<tokens.size()&&tokens[i]=="o") ++i;
    if(i<tokens.size()&&tokens[i]=="dia") ++i;
    if(i>=tokens.size()) return false;
    if(tokens[i]=="hoje"||tokens[i]=="amanha") {
        if(!clockValid) return false;
        date=addDays(today,tokens[i]=="amanha"?1:0);date.hour=date.minute=date.second=0;++i;
        if(!validCivil(date)) return false;
    } else if(tokens[i].find('/')!=std::string::npos) {
        if(!parseCivil(tokens[i++]+" 00:00",date)) return false;
    } else {
        if(i+4>=tokens.size()||!digits(tokens[i],date.day)||tokens[i+1]!="de"||tokens[i+3]!="de"||
           tokens[i+4].size()!=4||!digits(tokens[i+4],date.year)) return false;
        const char *months[]={"janeiro","fevereiro","marco","abril","maio","junho","julho","agosto","setembro","outubro","novembro","dezembro"};
        date.month=0;for(int m=0;m<12;++m) if(tokens[i+2]==months[m]) date.month=m+1;
        i+=5;date.hour=date.minute=date.second=0;
        if(!validCivil(date)) return false;
    }
    const size_t timeStart=i;
    while(i<tokens.size()&&tokens[i]!="para"&&tokens[i]!="pra") ++i;
    if(i>=tokens.size()||i+1>=tokens.size()) return false;
    hasTime=i>timeStart;
    if(hasTime) {
        std::string time;for(size_t t=timeStart;t<i;++t) {if(!time.empty()) time+=' ';time+=tokens[t];}
        if(!eventTime(time,date)) return false;
    }
    title=trim(payload.substr(rawOffset(payload,ends[i])));return !title.empty();
}
std::string eventPreview(const std::string &title,int64_t utc,int zone,bool reminder) {
    return std::string(reminder?"Criar lembrete: ":"Criar evento: ")+title+"\n"+formatCivil(utc,zone)+
        (reminder?"\nAviso no horário.":"\nSem aviso sonoro.")+"\nDigite sim ou cancelar.";
}
constexpr size_t pageBytes=384;
constexpr size_t helpPages=6;
const char *helpPage(size_t page) {
    static const char *pages[]={
        "Posso fazer no PDA:\nNotas: criar, listar, ler, editar, renomear e excluir.\nAgenda: criar eventos/lembretes, consultar, remarcar, editar e excluir.\nCalendário: dias da semana, intervalos e dias úteis.\nTimer: iniciar, consultar, alterar e cancelar.\nContatos: cadastrar e consultar telefone.\nTambém calculo contas e mostro bateria e hora.\nMudanças pedem sim; cancelar descarta.\nDigite mais ou ajuda notas, ajuda agenda, ajuda calendario, ajuda timer, ajuda contatos.",
        "Ajuda: notas\nCriar: nota: comprar leite\nTambém: pode criar uma nota sobre comprar pão.\nListar: minhas notas\nBuscar: busca notas Compras\nLer: le nota 1\nEditar todo o texto: edita essa nota: novo texto\nTítulo: renomeia essa nota para Compras\nExcluir: apaga essa nota\nUse a posição da lista (sem #). Essa nota se refere à última lida.\nDigite mais para ver agenda.",
        "Ajuda: agenda\nCriar: evento Dentista 18/11/2026 09:00\nTambém: crie um evento Comprar pão amanhã às 9h.\nSem horário, pergunto antes da prévia.\nAviso: lembrete Leite 18/11/2026 18:00\nListar: meus eventos\nConsultar: agenda hoje, agenda amanha ou qual meu proximo evento?\nLer: le evento 1\nRemarcar: remarca esse evento para 20/11/2026 10:30\nDetalhes: edita esse evento: levar exames\nTítulo: renomeia esse evento para Consulta\nExcluir: exclui esse evento\nCriar/remarcar exige relógio ajustado.\nDigite mais para ver timer.",
        "Ajuda: timer\nIniciar: timer de 5 segundos\nTambém: timer meia hora, timer vinte e cinco segundos ou uma hora e meia\nConsultar: quanto tempo falta no timer?\nAlterar restante: altera o timer para 5 minutos\nCancelar contagem: cancela o timer\nRevise a prévia e envie sim ou cancelar. Alterar substitui o restante.\nAo terminar, toca até fechar o aviso com OK, Enter ou trackball, se Som estiver ligado.\nDigite mais para ver outras funções.",
        "Ajuda: contatos, contas e sistema\nCadastrar: contato Oficina: 11 99999-8888\nTelefone: qual o telefone de Tamires?\nConta: quanto e 288 dividido por 32,66\nTambém: quanto e 12 vezes 5.3\nBateria: qnt % de bateria ainda tem?\nHora: q horas sao\nMais de um contato: informe o nome completo.\nLeituras longas: mais. Limpar apaga a conversa e o pedido pendente, sem desfazer mudanças confirmadas.\nDigite mais para ver cálculos de calendário.",
        "Ajuda: calendário\nSemana: quando cai dia 8 de novembro de 2026\nTambém: quando cai dia 8 de novembro ou quando cai dia 8\nIntervalo: do dia 1 de novembro ate 25 de dezembro sao quantos dias\nSomar: dia 8 de novembro mais 5 dias corridos\nTambém: dia 8 de novembro mais 5 dias uteis\nDatas incompletas usam o relógio do PDA. Dias úteis: segunda a sexta, sem feriados. A contagem exclui o dia inicial.\nDigite ajuda para voltar ao resumo."
    };
    return page<helpPages?pages[page]:"Fim da ajuda. Digite ajuda para voltar ao resumo.";
}
size_t pageEnd(const std::string &text,size_t offset,size_t count=pageBytes) {
    size_t end=std::min(text.size(),offset+count);
    while(end>offset&&end<text.size()&&(uint8_t(text[end])&0xc0)==0x80) --end;
    return end;
}
std::string excerpt(const std::string &text,size_t count=120) {
    const auto end=pageEnd(text,0,count);
    return text.substr(0,end)+(end<text.size()?"...":"");
}
template<class T> std::vector<uint32_t> itemIds(const std::vector<T> &items) {
    std::vector<uint32_t> ids;ids.reserve(items.size());for(const auto &item:items) ids.push_back(item.id);return ids;
}
template<class T> size_t positionOf(const std::vector<T> &items,uint32_t id) {
    for(size_t i=0;i<items.size();++i) if(items[i].id==id) return i+1;
    return 0;
}
std::string noteHeading(const Note &n,size_t position) {return "Nota "+std::to_string(position)+": "+excerpt(n.title,80);}
std::string eventHeading(const Appointment &a,int zone,size_t position) {
    return "Evento "+std::to_string(position)+": "+excerpt(a.title,80)+"\n"+formatCivil(a.start,zone);
}
bool sameNote(const Note &a,const Note &b) {return a.id==b.id&&a.title==b.title&&a.text==b.text;}
bool sameEvent(const Appointment &a,const Appointment &b) {
    return a.id==b.id&&a.title==b.title&&a.detail==b.detail&&a.start==b.start&&a.end==b.end&&
        a.reminderMinutes==b.reminderMinutes&&a.alerted==b.alerted&&a.draft==b.draft;
}
uint32_t readStamp(const std::string &text,const std::string &title) {
    return crc32(reinterpret_cast<const uint8_t *>(text.data()),text.size())^
        crc32(reinterpret_cast<const uint8_t *>(title.data()),title.size());
}
void invalidateEditor(State &state,uint32_t id,bool note) {
    auto &v=state.session;
    if(v.document!=id || (note?v.app!=int8_t(AppId::Notes):(v.app!=int8_t(AppId::Agenda)&&v.app!=int8_t(AppId::Calendar)))) return;
    v.document=0;v.view=0;v.form=0;v.field=0;v.caret=0;v.selected=0;v.editing=false;v.draft.clear();
}
template<class T> std::vector<uint32_t> matching(const std::vector<T> &items,const std::string &query) {
    auto key=fold(query);std::vector<uint32_t> result;
    if(!key.empty()&&key.back()=='?') key=trim(key.substr(0,key.size()-1));
    if(!key.empty()&&key[0]=='#') {
        if(key.size()==1||key[1]<'0'||key[1]>'9') return result;
        char *end=nullptr;const auto id=strtoul(key.c_str()+1,&end,10);
        if(!end||*end||id>UINT32_MAX) return result;
        for(const auto &item:items) if(item.id==id) result.push_back(item.id);
        return result;
    }
    if(!key.empty()) for(const auto &item:items) if(fold(item.title)==key) result.push_back(item.id);
    if(!result.empty()) return result;
    for(const auto &item:items) if(key.empty()||fold(item.title).find(key)!=std::string::npos) result.push_back(item.id);
    return result;
}
bool splitChange(std::string payload,std::string &selector,std::string &value) {
    const auto norm=fold(payload);auto pos=norm.find(": ");size_t skip=2;
    if(pos==std::string::npos) {pos=norm.find(" para ");skip=6;}
    if(pos==std::string::npos) return false;
    selector=trim(payload.substr(0,rawOffset(payload,pos)));
    value=trim(payload.substr(rawOffset(payload,pos+skip)));
    return !value.empty();
}
bool phoneValid(const std::string &s) {
    size_t digits=0;
    for (char c:s) {
        if (c>='0' && c<='9') ++digits;
        else if (c!=' ' && c!='+' && c!='-' && c!='(' && c!=')') return false;
    }
    return digits>=8 && digits<=15;
}
std::string secondsText(uint64_t ms) {
    auto sec=(ms+999)/1000;
    return std::to_string(sec/60)+" min "+std::to_string(sec%60)+" s";
}
bool duration(std::string s,uint64_t &ms) {
    s=trim(s);
    if (s=="meio minuto") {ms=30000;return true;}
    if (s=="meia hora") {ms=1800000;return true;}
    uint64_t sum=0;bool any=false;
    while (!s.empty()) {
        if (any && starts(s,"e ")) s=trim(s.substr(2));
        if (s.empty()) return false;
        double n=0;size_t consumed=0;
        if (s[0]>='0' && s[0]<='9') {
            // Decimal comma is accepted just as in the calculator.
            std::replace(s.begin(),s.end(),',','.');
            char *end=nullptr;n=strtod(s.c_str(),&end);consumed=size_t(end-s.c_str());
        } else {
            const std::pair<const char *,int> numbers[]={
                {"um",1},{"uma",1},{"dois",2},{"duas",2},{"tres",3},{"quatro",4},{"cinco",5},
                {"seis",6},{"sete",7},{"oito",8},{"nove",9},{"dez",10},{"onze",11},{"doze",12},
                {"treze",13},{"quatorze",14},{"catorze",14},{"quinze",15},{"dezesseis",16},
                {"dezessete",17},{"dezoito",18},{"dezenove",19},{"vinte",20},{"trinta",30},
                {"quarenta",40},{"cinquenta",50},{"sessenta",60},{"setenta",70},{"oitenta",80},{"noventa",90}};
            const auto space=s.find(' ');const auto token=s.substr(0,space);
            for(const auto &number:numbers) if(token==number.first) {n=number.second;consumed=token.size();break;}
            if(n>=20&&n<=90&&starts(s.substr(consumed)," e ")) {
                const auto rest=s.substr(consumed+3);const auto unit=rest.substr(0,rest.find(' '));
                for(const auto &number:numbers) if(number.second<10&&unit==number.first) {n+=number.second;consumed+=3+unit.size();break;}
            }
        }
        if (!std::isfinite(n) || n<=0 || n>31536000) return false;
        s=trim(s.substr(consumed));
        size_t count=0;while(count<s.size() && s[count]>='a' && s[count]<='z') ++count;
        const auto unit=s.substr(0,count);uint64_t scale=0;
        if(unit=="s"||unit=="seg"||unit=="segundo"||unit=="segundos") scale=1000;
        if(unit=="m"||unit=="min"||unit=="minuto"||unit=="minutos") scale=60000;
        if(unit=="h"||unit=="hora"||unit=="horas") scale=3600000;
        if(!scale) return false;
        const double part=n*scale;
        if (part<1 || part>31536000000.0 || part!=std::floor(part)) return false;
        sum+=uint64_t(part);any=true;s=trim(s.substr(count));
        if(s=="e meio"||s=="e meia") {sum+=scale/2;s.clear();}
        if(sum>31536000000ULL) return false;
    }
    ms=sum;return any;
}
bool timerRequest(std::string s,std::string &payload) {
    for(const char *verb:{"cria ","crie ","criar ","coloca ","coloque ","poe ","inicia ","inicie ","ajusta ","ajuste ","solta ","define ","defina ","bota "}) {
        if(starts(s,verb)) {s=trim(s.substr(std::char_traits<char>::length(verb)));break;}
    }
    if(starts(s,"uma ")) s=trim(s.substr(4));
    else if(starts(s,"um ")) s=trim(s.substr(3));
    else if(starts(s,"o ")) s=trim(s.substr(2));
    bool matched=false;
    for(const char *noun:{"timer ","temporizador ","contagem regressiva "}) {
        if(starts(s,noun)) {s=trim(s.substr(std::char_traits<char>::length(noun)));matched=true;break;}
    }
    if(!matched) return false;
    for(const char *connector:{"de ","para ","pra "}) {
        if(starts(s,connector)) {s=trim(s.substr(std::char_traits<char>::length(connector)));break;}
    }
    if(!s.empty() && (s.back()=='.'||s.back()=='!'||s.back()=='?')) s=trim(s.substr(0,s.size()-1));
    const std::string polite=" por favor";
    if(s.size()>=polite.size() && s.compare(s.size()-polite.size(),polite.size(),polite)==0)
        s=trim(s.substr(0,s.size()-polite.size()));
    payload=s;return true;
}
bool timerControl(std::string s,bool &cancel,std::string &payload) {
    bool found=false;
    for(const char *verb:{"cancela ","cancelar ","cancele ","para ","pare ","parar ","zera ","zerar ",
                         "altera ","alterar ","altere ","muda ","mudar ","mude ","atualiza "}) {
        if(starts(s,verb)) {
            cancel=starts(verb,"cancel")||starts(verb,"par")||starts(verb,"zer");
            s=trim(s.substr(std::char_traits<char>::length(verb)));found=true;break;
        }
    }
    if(!found) return false;
    if(starts(s,"o ")) s=trim(s.substr(2));else if(starts(s,"um ")) s=trim(s.substr(3));
    if(!s.empty()&&(s.back()=='.'||s.back()=='!'||s.back()=='?')) s=trim(s.substr(0,s.size()-1));
    const auto space=s.find(' ');const auto noun=s.substr(0,space);
    if(noun!="timer"&&noun!="temporizador") return false;
    const auto rest=space==std::string::npos?"":trim(s.substr(space+1));
    if(cancel) payload=rest;
    else if(!timerRequest("timer "+rest,payload)) payload.clear();
    return true;
}
bool calculate(std::string s,std::string &reply) {
    s=commandKey(s);
    for (const char *prefix:{"quanto e ","quanto da ","calcula ","calcule ","calcular ","qual o resultado de ","qual e o resultado de ","qual o valor de ","qual e o valor de ","resultado de ","quanto resulta ","quanto fica "}) {
        if(starts(s,prefix)) {s=s.substr(std::char_traits<char>::length(prefix));break;}
    }
    for(const auto &op:{std::make_pair("divide ","/"),std::make_pair("divida ","/"),std::make_pair("dividir ","/"),
                       std::make_pair("multiplica ","*"),std::make_pair("multiplique ","*"),std::make_pair("multiplicar ","*")}) {
        if(starts(s,op.first)) {
            s=s.substr(std::char_traits<char>::length(op.first));const auto p=s.find(" por ");
            if(p==std::string::npos) return false;
            s.replace(p,5,op.second);break;
        }
    }
    for(const char *v:{"soma ","some ","somar "}) if(starts(s,v)) {
        s=s.substr(std::char_traits<char>::length(v));auto p=s.find(" e ");size_t skip=3;
        if(p==std::string::npos) {p=s.find(" com ");skip=5;}
        if(p==std::string::npos) return false;
        s.replace(p,skip,"+");break;
    }
    for(const char *v:{"subtrai ","subtraia ","subtrair "}) if(starts(s,v)) {
        s=s.substr(std::char_traits<char>::length(v));const auto p=s.find(" de ");
        if(p==std::string::npos) return false;
        s="("+s.substr(p+4)+")-("+s.substr(0,p)+")";break;
    }
    if (!s.empty() && s.back()=='?') s.pop_back();
    const std::pair<const char *,const char *> ops[]={
        {"dividido por","/"},{"multiplicado por","*"},{"vezes","*"},{"mais","+"},{"menos","-"}};
    for (const auto &op:ops) {
        size_t p=0;while((p=s.find(op.first,p))!=std::string::npos) {s.replace(p,std::char_traits<char>::length(op.first),op.second);++p;}
    }
    bool digit=false,operation=false;
    for(char &c:s) {
        if(c==',') c='.';
        if(c>='0'&&c<='9') digit=true;
        else if(c=='+'||c=='-'||c=='*'||c=='/'||c=='('||c==')') operation=true;
        else if(c!='.'&&c!=' ') return false;
    }
    if(!digit||!operation) return false;
    const auto v=Evaluator().expression(s);
    reply=v.kind==ValueKind::Number?"Resultado: "+v.display():"Não consegui calcular: "+v.display();
    return true;
}
bool agendaQuery(std::string s,int &day,bool &next,bool &reminders) {
    s=commandKey(s);day=0;next=false;reminders=false;
    if(s=="qual meu proximo evento"||s=="qual o meu proximo evento"||s=="qual o proximo compromisso"||
       s=="qual meu proximo compromisso"||s=="qual o proximo compromisso da minha agenda"||s=="proximo evento"||
       s=="proximo compromisso"||s=="o que tenho a seguir na agenda") {next=true;return true;}
    for(const char *when:{"hoje","hj","amanha"}) {
        for(const char *pattern:{"agenda %s","agenda de %s","minha agenda %s","minha agenda de %s","agenda pra %s",
            "como esta minha agenda %s","como esta a minha agenda %s","mostra minha agenda %s","mostra minha agenda de %s","mostra a agenda de %s",
            "o que tenho na agenda %s","o que eu tenho na agenda %s","o que tenho agendado %s","o que tenho pra %s","o que tem no meu calendario %s",
            "tenho algo na agenda pra %s","tenho algo na agenda para %s","tenho algum evento agendado %s","tenho algum compromisso %s",
            "tenho compromissos %s","quais compromissos tenho %s","quais compromissos eu tenho %s","quais sao os eventos de %s",
            "meus compromissos de %s","mostra meus compromissos de %s","eventos de %s","lembretes %s","lembretes de %s",
            "tenho algum lembrete pra %s","tenho algum lembrete para %s","quais sao os meus lembretes de %s","quais sao meus lembretes de %s",
            "mostra os lembretes de %s","tem algum lembrete marcado para %s"}) {
            char value[128];snprintf(value,sizeof(value),pattern,when);
            if(s==value) {day=std::string(when)=="amanha"?1:0;reminders=s.find("lembrete")!=std::string::npos;return true;}
        }
    }
    return false;
}
}

int assistantTelemetryIntent(const std::string &text) {
    const auto s=commandKey(commandText(text));
    for(const char *q:{"bateria","status da bateria","como esta a bateria","quanto de bateria resta","quanto de bateria ainda tem",
        "quanto de bat tem ainda","qual a carga da bateria","qual o nivel da bateria","qual a porcentagem de bateria",
        "quanto tenho de bateria","quanto tenho de bateria ainda","qnt % de bateria ainda tem","quanto resta de bateria",
        "mostra a bateria","mostra o nivel da bateria","me diga a bateria"}) if(s==q) return 1;
    for(const char *q:{"hora","horas","que horas sao","que horas sao agora","q horas sao","q horas sao agora",
        "qual a hora","qual e a hora","qual o horario","qual o horario agora","qual a hora atual","me diga a hora",
        "mostra a hora","me diz que horas sao"}) if(s==q) return 2;
    return 0;
}

std::string PdaServices::persist(const std::string &success) {
    sys_.changed(true);
    return sys_.save()?success:success+" Apenas na memória: falha ao gravar; o PDA tentará novamente.";
}
std::string PdaServices::addNote(const std::string &text) {
    if(trim(text).empty()||text.size()>limits::noteBytes) return "Texto da nota inválido.";
    if(sys_.state.notes.size()>=limits::notes) return "Limite de notas atingido.";
    Note n;n.id=sys_.state.allocateId();n.text=text;
    n.title=text.substr(0,48);while(!n.title.empty() && (uint8_t(text[n.title.size()])&0xc0)==0x80) n.title.pop_back();
    sys_.state.notes.push_back(n);return persist("Nota criada.");
}
std::string PdaServices::addContact(const std::string &name,const std::string &phone) {
    if(trim(name).empty()||name.size()>128||!phoneValid(phone)) return "Nome ou telefone inválido.";
    if(sys_.state.contacts.size()>=limits::contacts) return "Limite de contatos atingido.";
    for(const auto &c:sys_.state.contacts) if(fold(c.name)==fold(name)) return "Já existe um contato com esse nome. Use Contatos para editar.";
    Contact c;c.id=sys_.state.allocateId();c.name=name;c.phone=phone;
    sys_.state.contacts.push_back(c);return persist("Contato criado.");
}
std::string PdaServices::addEvent(const std::string &title,int64_t utc,bool reminder) {
    if(trim(title).empty()||title.size()>256||utc<946684800||utc>4102444799LL) return "Título ou data inválida.";
    if(!sys_.clockValid()) return "Ajuste a data e a hora em Relógio antes de cadastrar na agenda.";
    if(utc<=sys_.utc()) return "Essa data já passou. Informe uma data futura.";
    if(sys_.state.appointments.size()>=limits::events) return "Limite de eventos atingido.";
    Appointment a;a.id=sys_.state.allocateId();a.title=title;a.start=utc;a.reminderMinutes=reminder?0:-1;
    sys_.state.appointments.push_back(a);return persist(reminder?"Lembrete criado para o horário informado.":"Evento criado, sem aviso sonoro.");
}
std::string PdaServices::startTimer(uint64_t ms) {
    if(!ms||ms>31536000000ULL) return "Duração inválida.";
    sys_.startTimer(ms);return persist("Timer iniciado: "+secondsText(ms)+".");
}
std::vector<uint32_t> PdaServices::findNotes(const std::string &query) const {return matching(sys_.state.notes,query);}
std::vector<uint32_t> PdaServices::findEvents(const std::string &query) const {return matching(sys_.state.appointments,query);}
std::string PdaServices::readNote(uint32_t id,size_t offset,size_t position) const {
    const auto *n=findById(sys_.state.notes,id);
    if(!n) return "Nota não encontrada.";
    if(offset>n->text.size()) return "Fim da nota.";
    const auto end=pageEnd(n->text,offset);
    return noteHeading(*n,position?position:positionOf(sys_.state.notes,id))+"\n"+n->text.substr(offset,end-offset)+(end<n->text.size()?"\nDigite mais para continuar.":"");
}
std::string PdaServices::readEvent(uint32_t id,size_t offset,size_t position) const {
    const auto *a=findById(sys_.state.appointments,id);
    if(!a) return "Evento não encontrado.";
    std::string out=eventHeading(*a,sys_.state.preferences.timezoneMinutes,position?position:positionOf(sys_.state.appointments,id));
    if(a->end) out+="\nFim: "+formatCivil(a->end,sys_.state.preferences.timezoneMinutes);
    out+=a->reminderMinutes<0?"\nSem aviso sonoro.":"\nLembrete: "+std::to_string(a->reminderMinutes)+" min antes.";
    if(!a->draft.empty()) out+="\nHá um rascunho em edição na Agenda.";
    if(offset>a->detail.size()) return "Fim dos detalhes.";
    const auto end=pageEnd(a->detail,offset);
    if(!a->detail.empty()) out+="\n"+a->detail.substr(offset,end-offset);
    if(end<a->detail.size()) out+="\nDigite mais para continuar.";
    return out;
}
std::string PdaServices::editNote(const Note &expected,const std::string &value,bool rename) {
    auto *n=findById(sys_.state.notes,expected.id);
    if(!n||!sameNote(*n,expected)) return "A nota mudou ou foi excluída. Leia novamente antes de alterar.";
    if(trim(value).empty()||value.size()>(rename?256:limits::noteBytes)) return "Texto ou título inválido.";
    if(rename) n->title=value;else n->text=value;
    invalidateEditor(sys_.state,n->id,true);return persist(rename?"Título da nota alterado.":"Texto da nota alterado.");
}
std::string PdaServices::deleteNote(const Note &expected) {
    auto *n=findById(sys_.state.notes,expected.id);
    if(!n||!sameNote(*n,expected)) return "A nota mudou ou foi excluída. Leia novamente antes de excluir.";
    invalidateEditor(sys_.state,n->id,true);
    auto &items=sys_.state.notes;items.erase(std::remove_if(items.begin(),items.end(),[&](const Note &x){return x.id==expected.id;}),items.end());
    return persist("Nota excluída.");
}
std::string PdaServices::editEvent(const Appointment &expected,const Appointment &replacement) {
    auto *a=findById(sys_.state.appointments,expected.id);
    if(!a||!sameEvent(*a,expected)) return "O evento mudou ou foi excluído. Leia novamente antes de alterar.";
    if(!a->draft.empty()) return "Conclua o rascunho desse evento na Agenda antes de alterar pelo chat.";
    if(replacement.id!=expected.id||trim(replacement.title).empty()||replacement.title.size()>256||replacement.detail.size()>2048||
       !validCivil(fromUtc(replacement.start,0))||(replacement.end&&(replacement.end<replacement.start||!validCivil(fromUtc(replacement.end,0))))||
       replacement.reminderMinutes < -1||replacement.reminderMinutes>10080||!replacement.draft.empty()) return "Dados do evento inválidos.";
    if(replacement.start!=expected.start && (!sys_.clockValid()||replacement.start<=sys_.utc()))
        return "Ajuste o relógio e informe uma data futura para remarcar.";
    *a=replacement;invalidateEditor(sys_.state,a->id,false);return persist("Evento alterado.");
}
std::string PdaServices::deleteEvent(const Appointment &expected) {
    auto *a=findById(sys_.state.appointments,expected.id);
    if(!a||!sameEvent(*a,expected)) return "O evento mudou ou foi excluído. Leia novamente antes de excluir.";
    if(!a->draft.empty()) return "Conclua o rascunho desse evento na Agenda antes de excluir pelo chat.";
    invalidateEditor(sys_.state,a->id,false);
    auto &items=sys_.state.appointments;items.erase(std::remove_if(items.begin(),items.end(),[&](const Appointment &x){return x.id==expected.id;}),items.end());
    return persist("Evento excluído.");
}
std::string PdaServices::changeTimer(uint64_t ms,uint32_t revision) {
    if(sys_.timerRevision()!=revision) return "O timer mudou. Consulte novamente antes de alterar.";
    if(!sys_.state.timerMs) return "Não há timer para alterar.";
    if(!ms||ms>31536000000ULL) return "Duração inválida.";
    const bool paused=!sys_.state.timerRunning;
    sys_.startTimer(ms);if(paused) sys_.pauseTimer();
    return persist("Tempo restante alterado para "+secondsText(ms)+(paused?". Timer continua pausado.":"."));
}
std::string PdaServices::cancelTimer(uint32_t revision) {
    if(sys_.timerRevision()!=revision) return "O timer mudou. Consulte novamente antes de cancelar.";
    if(!sys_.state.timerMs&&!sys_.timerAlertActive()) return "Não há timer para cancelar.";
    sys_.startTimer(0);if(sys_.timerAlertActive()) sys_.dismissNotification();
    return persist("Timer cancelado.");
}
std::string PdaServices::timer() const {
    if(sys_.timerAlertActive()) return "O timer terminou e aguarda fechar o aviso.";
    if(sys_.state.timerRunning) return "Faltam "+secondsText(sys_.state.timerMs)+" no timer.";
    return sys_.state.timerMs?"Timer pausado em "+secondsText(sys_.state.timerMs)+".":"Não há timer em andamento.";
}
std::string PdaServices::contact(const std::string &name) const {
    const auto key=fold(name);const Contact *match=nullptr;size_t count=0;
    for(const auto &c:sys_.state.contacts) if(fold(c.name)==key) {match=&c;++count;}
    if(!count) for(const auto &c:sys_.state.contacts) if(fold(c.name).find(key)!=std::string::npos) {match=&c;++count;}
    if(count>1) return "Encontrei mais de um contato. Informe o nome completo.";
    if(!match||key.empty()) return "Contato não encontrado. Busco pelo nome salvo; parentescos precisam estar no nome.";
    return match->name+": "+(match->phone.empty()?"sem telefone cadastrado":match->phone);
}
std::vector<uint32_t> PdaServices::agendaItems(int offset,bool next,bool reminders) const {
    if(!sys_.clockValid()) return {};
    const auto now=sys_.utc();auto local=addDays(fromUtc(now,sys_.state.preferences.timezoneMinutes),offset);
    local.hour=local.minute=local.second=0;const auto start=toUtc(local,sys_.state.preferences.timezoneMinutes);
    std::vector<const Appointment *> list;
    for(const auto &a:sys_.state.appointments) {
        if(!a.draft.empty()||!a.start||(reminders&&a.reminderMinutes<0)) continue;
        if(next?a.start>=now:(a.start>=start && a.start<start+86400)) list.push_back(&a);
    }
    std::sort(list.begin(),list.end(),[](const Appointment *a,const Appointment *b){return a->start<b->start;});
    std::vector<uint32_t> ids;for(const auto *a:list) {ids.push_back(a->id);if(next) break;}return ids;
}
std::string PdaServices::agenda(int offset,bool next,bool reminders) const {
    if(!sys_.clockValid()) return "Ajuste a data e a hora em Relógio para consultar a agenda.";
    const auto ids=agendaItems(offset,next,reminders);
    if(ids.empty()) return "Nenhum compromisso encontrado para esse período.";
    std::string out;const size_t limit=next?1:5;
    for(size_t i=0;i<ids.size()&&i<limit;++i) {
        if(i) out+='\n';
        out+=eventHeading(*findById(sys_.state.appointments,ids[i]),sys_.state.preferences.timezoneMinutes,i+1);
    }
    if(!next && ids.size()>limit) out+="\nMais "+std::to_string(ids.size()-limit)+" na Agenda.";
    return out;
}
void Assistant::rememberPositions(System &sys,bool note,std::vector<uint32_t> ids) {
    auto &positions=note?notePositions_:eventPositions_;
    positions.known=true;positions.ids=std::move(ids);
    positions.order=note?itemIds(sys.state.notes):itemIds(sys.state.appointments);
}
bool Assistant::positionsValid(const System &sys,bool note) const {
    const auto &positions=note?notePositions_:eventPositions_;
    return positions.known&&positions.order==(note?itemIds(sys.state.notes):itemIds(sys.state.appointments));
}
size_t Assistant::displayPosition(const System &sys,bool note,uint32_t id) const {
    const auto &positions=note?notePositions_:eventPositions_;
    if(positionsValid(sys,note)) {
        const auto found=std::find(positions.ids.begin(),positions.ids.end(),id);
        if(found!=positions.ids.end()) return size_t(found-positions.ids.begin())+1;
    }
    return note?positionOf(sys.state.notes,id):positionOf(sys.state.appointments,id);
}
std::string Assistant::readPage(System &sys) {
    PdaServices api(sys);
    if(cursor_==Cursor::Help) {
        const auto reply=helpPage(offset_++);if(offset_>=helpPages) cursor_=Cursor::None;
        return reply;
    }
    if(cursor_==Cursor::Note||cursor_==Cursor::Event) {
        const bool note=cursor_==Cursor::Note;
        const auto *n=note?findById(sys.state.notes,contextId_):nullptr;
        const auto *a=note?nullptr:findById(sys.state.appointments,contextId_);
        if((note&&!n)||(!note&&!a)) {cursor_=Cursor::None;contextId_=0;return "O item foi excluído. Faça outra busca.";}
        const auto &text=note?n->text:a->detail;const auto &title=note?n->title:a->title;
        if(readStamp_!=readStamp(text,title)) {cursor_=Cursor::None;return "O item mudou durante a leitura. Leia novamente desde o início.";}
        const auto position=displayPosition(sys,note,contextId_);
        const auto reply=note?api.readNote(contextId_,offset_,position):api.readEvent(contextId_,offset_,position);
        offset_=pageEnd(text,offset_);if(offset_>=text.size()) cursor_=Cursor::None;
        return reply;
    }
    if(cursor_==Cursor::Notes||cursor_==Cursor::Events) {
        const bool note=cursor_==Cursor::Notes;
        if(!positionsValid(sys,note)) {cursor_=Cursor::None;return note?"A lista mudou. Peça minhas notas para atualizar as posições.":"A lista mudou. Peça meus eventos para atualizar as posições.";}
        const auto &ids=(note?notePositions_:eventPositions_).ids;
        if(offset_>=ids.size()) {cursor_=Cursor::None;return note?"Nenhuma outra nota encontrada.":"Nenhum outro evento encontrado.";}
        std::string out=note?"Notas encontradas:":"Eventos encontrados:";
        const auto end=std::min(ids.size(),offset_+5);
        for(size_t i=offset_;i<end;++i) {
            if(note) out+="\n"+noteHeading(*findById(sys.state.notes,ids[i]),i+1);
            else out+="\n"+eventHeading(*findById(sys.state.appointments,ids[i]),sys.state.preferences.timezoneMinutes,i+1);
        }
        offset_=end;if(end<ids.size()) out+="\nDigite mais para continuar.";else cursor_=Cursor::None;
        out+=note?"\nPara ler, use a posição: le nota 1.":"\nPara ler, use a posição: le evento 1.";
        return out;
    }
    return "Não há mais conteúdo nessa leitura. Faça outra busca.";
}
std::string Assistant::records(System &sys,const std::string &raw,const std::string &s) {
    if(s=="mais"||s=="continua"||s=="continuar"||s=="proxima pagina") return readPage(sys);
    enum class Verb { Read, List, Search, Erase, Edit, Change, Rename };
    Verb verb=Verb::Read;std::string rest;bool recognized=false;
    static const std::pair<const char *,Verb> verbs[]={
        {"le ",Verb::Read},{"leia ",Verb::Read},{"ler ",Verb::Read},{"mostra ",Verb::Read},{"mostrar ",Verb::Read},{"ver ",Verb::Read},
        {"lista ",Verb::List},{"listar ",Verb::List},{"liste ",Verb::List},
        {"busca ",Verb::Search},{"buscar ",Verb::Search},{"procura ",Verb::Search},{"procure ",Verb::Search},{"pesquisa ",Verb::Search},
        {"apaga ",Verb::Erase},{"apagar ",Verb::Erase},{"apague ",Verb::Erase},{"exclui ",Verb::Erase},{"excluir ",Verb::Erase},{"exclua ",Verb::Erase},
        {"deleta ",Verb::Erase},{"deletar ",Verb::Erase},{"remove ",Verb::Erase},{"remover ",Verb::Erase},{"cancela ",Verb::Erase},{"cancelar ",Verb::Erase},
        {"edita ",Verb::Edit},{"editar ",Verb::Edit},{"edite ",Verb::Edit},
        {"altera ",Verb::Change},{"alterar ",Verb::Change},{"altere ",Verb::Change},{"muda ",Verb::Change},{"mudar ",Verb::Change},{"atualiza ",Verb::Change},
        {"remarca ",Verb::Change},{"remarcar ",Verb::Change},{"reagenda ",Verb::Change},
        {"renomeia ",Verb::Rename},{"renomear ",Verb::Rename},{"renomeie ",Verb::Rename}};
    for(const auto &v:verbs) {
        if(prefixPayload(raw,s,v.first,rest)) {verb=v.second;recognized=true;break;}
    }
    if(!recognized) {
        if(s=="notas"||s=="minhas notas"||s=="quais sao minhas notas"||s=="quais sao minhas notas?"||
           s=="eventos"||s=="meus eventos"||s=="minha agenda") {
            rest=(s.find("nota")!=std::string::npos)?"notas":"eventos";verb=Verb::List;
        } else return "";
    }
    if(verb==Verb::Read||verb==Verb::List||verb==Verb::Search||verb==Verb::Erase)
        rest=trim(rest.substr(0,rawOffset(rest,commandKey(rest,false).size())));
    bool contextual=false;
    for(const char *article:{"a ","o ","as ","os ","minhas ","meus ","minha ","meu ","essa ","esse ","esta ","este "}) {
        if(starts(fold(rest),article)) {
            contextual=starts(article,"ess")||starts(article,"est");
            rest=trim(rest.substr(rawOffset(rest,std::char_traits<char>::length(article))));break;
        }
    }
    const std::pair<const char *,int> ordinals[]={{"primeira",1},{"primeiro",1},{"segunda",2},{"segundo",2},{"terceira",3},{"terceiro",3},
        {"quarta",4},{"quarto",4},{"quinta",5},{"quinto",5},{"sexta",6},{"sexto",6},{"setima",7},{"setimo",7},{"oitava",8},{"oitavo",8},
        {"nona",9},{"nono",9},{"decima",10},{"decimo",10}};
    for(const auto &o:ordinals) {
        const auto p=std::string(o.first)+" ";const auto r=fold(rest);
        if(!starts(r,p.c_str())) continue;
        const auto noun=r.substr(p.size());
        if(noun=="nota"||noun=="evento"||noun=="compromisso"||noun=="lembrete") rest=noun+" "+std::to_string(o.second);
        break;
    }
    const auto norm=fold(rest);const auto space=norm.find(' ');auto noun=norm.substr(0,space);
    const bool colonAfterNoun=!noun.empty()&&noun.back()==':';if(colonAfterNoun) noun.pop_back();
    const bool note=noun=="nota"||noun=="notas"||noun=="anotacao"||noun=="anotacoes";
    const bool event=noun=="evento"||noun=="eventos"||noun=="compromisso"||noun=="compromissos"||noun=="lembrete"||noun=="lembretes";
    if(!note&&!event) return "";
    const bool plural=!noun.empty()&&noun.back()=='s';
    auto selector=space==std::string::npos?"":trim(rest.substr(rawOffset(rest,space+1)));
    PdaServices api(sys);
    if(plural&&commandKey(selector).empty()) selector.clear();
    if(plural&&(fold(selector)=="por favor"||fold(selector)==", por favor")) selector.clear();
    if(verb==Verb::List||verb==Verb::Search||(verb==Verb::Read&&plural&&selector.empty())) {
        contextId_=0;offset_=0;query_=selector;cursor_=note?Cursor::Notes:Cursor::Events;
        rememberPositions(sys,note,note?api.findNotes(query_):api.findEvents(query_));
        return readPage(sys);
    }
    std::string value;
    if(verb==Verb::Edit||verb==Verb::Change||verb==Verb::Rename) {
        if(colonAfterNoun&&!contextual) return "Informe o título ou posição antes de : para escolher o item.";
        if(contextual) selector=(colonAfterNoun?"essa: ":"essa ")+selector;
        if(!splitChange(selector,selector,value)) return note?"Use edita nota 1: novo texto ou renomeia nota 1 para Título.":"Use remarca evento 1 para DD/MM/AAAA HH:MM, edita evento 1: detalhes ou renomeia evento 1 para Título.";
    } else if(contextual && selector.empty()) selector="essa";
    auto key=fold(selector);
    if(verb==Verb::Read||verb==Verb::Erase) {
        bool exact=false;
        if(note) for(const auto &n:sys.state.notes) if(fold(n.title)==key) exact=true;
        if(event) for(const auto &a:sys.state.appointments) if(fold(a.title)==key) exact=true;
        if(!exact) {const auto cleaned=commandKey(selector);selector=selector.substr(0,rawOffset(selector,cleaned.size()));key=cleaned;}
    }
    for(const auto &o:ordinals) if(key==o.first) {selector=key=std::to_string(o.second);break;}
    if(key=="essa"||key=="esse"||key=="esta"||key=="este") {
        if(!contextId_||contextNote_!=note) return "Leia um item primeiro ou informe o título ou posição.";
        selector="#"+std::to_string(contextId_);
    }
    if(trim(selector).empty()) return note?"Qual nota? Use minhas notas para listar ou le nota 1.":"Qual evento? Use meus eventos para listar ou le evento 1.";
    std::vector<uint32_t> ids;
    if(!key.empty()&&key.back()=='?') key=trim(key.substr(0,key.size()-1));
    if(starts(key,"posicao ")) key=trim(key.substr(8));
    const bool byPosition=!key.empty()&&key.find_first_not_of("0123456789")==std::string::npos;
    if(byPosition) {
        auto &positions=note?notePositions_:eventPositions_;
        if(!positions.known) rememberPositions(sys,note,note?itemIds(sys.state.notes):itemIds(sys.state.appointments));
        if(!positionsValid(sys,note)) return note?"A lista mudou. Peça minhas notas para atualizar as posições.":"A lista mudou. Peça meus eventos para atualizar as posições.";
        const auto position=strtoul(key.c_str(),nullptr,10);
        if(!position||position>positions.ids.size()) return "Essa posição não existe na lista. Consulte a lista novamente.";
        ids.push_back(positions.ids[position-1]);
    } else ids=note?api.findNotes(selector):api.findEvents(selector);
    if(ids.size()!=1) {
        contextId_=0;cursor_=Cursor::None;
        if(ids.empty()) return note?"Nota não encontrada. Use minhas notas para listar.":"Evento não encontrado. Use meus eventos para listar.";
        rememberPositions(sys,note,ids);cursor_=note?Cursor::Notes:Cursor::Events;offset_=0;
        return "Há mais de um resultado. Repita o pedido com a posição da lista:\n"+readPage(sys);
    }
    auto &positions=note?notePositions_:eventPositions_;
    if(!positionsValid(sys,note)||std::find(positions.ids.begin(),positions.ids.end(),ids[0])==positions.ids.end())
        rememberPositions(sys,note,note?itemIds(sys.state.notes):itemIds(sys.state.appointments));
    contextId_=ids[0];contextNote_=note;offset_=0;cursor_=Cursor::None;
    if(verb==Verb::Read) {
        if(note) {const auto *n=findById(sys.state.notes,ids[0]);readStamp_=readStamp(n->text,n->title);cursor_=Cursor::Note;}
        else {const auto *a=findById(sys.state.appointments,ids[0]);readStamp_=readStamp(a->detail,a->title);cursor_=Cursor::Event;}
        return readPage(sys);
    }
    if(note) {
        noteBefore_=*findById(sys.state.notes,ids[0]);first_=value;
        if(verb==Verb::Erase) {
            pending_=Pending::DeleteNote;
            return "Excluir definitivamente "+noteHeading(noteBefore_,displayPosition(sys,true,noteBefore_.id))+"?\n"+excerpt(noteBefore_.text)+"\nDigite sim ou cancelar.";
        }
        pending_=verb==Verb::Rename?Pending::RenameNote:Pending::EditNote;
        return noteHeading(noteBefore_,displayPosition(sys,true,noteBefore_.id))+(verb==Verb::Rename?"\nNovo título: ":"\nSubstituir TODO o texto por:\n")+value+"\nDigite sim ou cancelar.";
    }
    eventBefore_=*findById(sys.state.appointments,ids[0]);eventAfter_=eventBefore_;
    if(!eventBefore_.draft.empty()) return "Conclua o rascunho desse evento na Agenda antes de editar ou excluir pelo chat.";
    if(verb==Verb::Erase) {
        pending_=Pending::DeleteEvent;return "Excluir definitivamente "+eventHeading(eventBefore_,sys.state.preferences.timezoneMinutes,displayPosition(sys,false,eventBefore_.id))+"?\nDigite sim ou cancelar.";
    }
    std::string change;
    if(verb==Verb::Rename) {eventAfter_.title=value;change="\nNovo título: "+value;}
    else if(verb==Verb::Edit) {eventAfter_.detail=value;change="\nSubstituir todos os detalhes por:\n"+value;}
    else {
        CivilTime date;bool hasTime=true;std::string ignored;
        if(!sys.clockValid()) return "Ajuste a data e a hora em Relógio antes de remarcar.";
        if(!parseCivil(value,date)&&!eventDetails(value+" para _",date,ignored,hasTime,true,fromUtc(sys.utc(),sys.state.preferences.timezoneMinutes)))
            return "Informe a data completa: remarca evento 1 para DD/MM/AAAA HH:MM, ou para amanhã às 9h.";
        if(!hasTime) {
            if(dateKey(date)<dateKey(fromUtc(sys.utc(),sys.state.preferences.timezoneMinutes))) return "Essa data já passou. Informe uma data futura.";
            date_=toUtc(date,0);pending_=Pending::RescheduleTime;
            return "Remarcar: "+eventBefore_.title+"\n"+formatCivil(date_,0,false)+"\nQual horário? Responda 09:00 ou 9h; cancelar descarta.";
        }
        eventAfter_.start=toUtc(date,sys.state.preferences.timezoneMinutes);
        if(eventAfter_.start<=sys.utc()) return "Essa data já passou. Informe uma data futura.";
        if(eventBefore_.end) eventAfter_.end=eventAfter_.start+(eventBefore_.end-eventBefore_.start);
        if(eventAfter_.end&&!validCivil(fromUtc(eventAfter_.end,0))) return "O fim do evento ultrapassa o intervalo de datas permitido.";
        eventAfter_.alerted=false;
        change="\nNovo início: "+formatCivil(eventAfter_.start,sys.state.preferences.timezoneMinutes);
        if(eventAfter_.end) change+="\nNovo fim: "+formatCivil(eventAfter_.end,sys.state.preferences.timezoneMinutes);
        change+="\nDuração e lembrete mantidos.";
    }
    pending_=Pending::EditEvent;
    return eventHeading(eventBefore_,sys.state.preferences.timezoneMinutes,displayPosition(sys,false,eventBefore_.id))+change+"\nDigite sim ou cancelar.";
}
void Assistant::resetDialogue() {
    pending_=Pending::None;first_.clear();second_.clear();date_=0;duration_=0;reminder_=false;
    noteBefore_=Note();eventBefore_=Appointment();eventAfter_=Appointment();timerRevision_=0;
    cursor_=Cursor::None;offset_=0;query_.clear();contextId_=readStamp_=0;
}
void Assistant::clear() {
    resetDialogue();notePositions_=Positions();eventPositions_=Positions();
}
std::string Assistant::respond(System &sys,const std::string &input) {
    const auto raw=commandText(input),s=fold(raw);const auto key=commandKey(s);PdaServices api(sys);
    if(key=="cancelar"||key=="cancela"||key=="cancele"||key=="nao"||key=="deixa pra la"||key=="esquece") {resetDialogue();return "Pedido cancelado.";}
    if(pending_==Pending::EventTime||pending_==Pending::RescheduleTime) {
        auto date=fromUtc(date_,0); // Civil date stored independently of the current timezone.
        if(!eventTime(s,date)) return "Qual horário? Responda 09:00, 9h ou às 9 da manhã; cancelar descarta.";
        if(!sys.clockValid()) return "Ajuste a data e a hora em Relógio antes de cadastrar na agenda.";
        const auto utc=toUtc(date,sys.state.preferences.timezoneMinutes);
        if(utc<=sys.utc()) {resetDialogue();return "Essa data já passou. Informe uma data futura.";}
        if(pending_==Pending::RescheduleTime) {
            eventAfter_.start=utc;
            if(eventBefore_.end) eventAfter_.end=utc+(eventBefore_.end-eventBefore_.start);
            if(eventAfter_.end&&!validCivil(fromUtc(eventAfter_.end,0))) {resetDialogue();return "O fim do evento ultrapassa o intervalo de datas permitido.";}
            eventAfter_.alerted=false;pending_=Pending::EditEvent;
            return "Remarcar: "+eventBefore_.title+"\nNovo início: "+formatCivil(utc,sys.state.preferences.timezoneMinutes)+"\nDuração e lembrete mantidos.\nDigite sim ou cancelar.";
        }
        date_=utc;pending_=Pending::Event;
        return eventPreview(first_,date_,sys.state.preferences.timezoneMinutes,reminder_);
    }
    if(pending_!=Pending::None) {
        if(key!="sim" && key!="confirmar"&&key!="confirmo"&&key!="pode confirmar") return "Há um pedido aguardando revisão. Digite sim para confirmar ou cancelar para descartar.";
        const auto kind=pending_;const auto a=first_,b=second_;const auto date=date_;const auto ms=duration_;const bool reminder=reminder_;
        const auto oldNote=std::move(noteBefore_);const auto oldEvent=std::move(eventBefore_);const auto newEvent=std::move(eventAfter_);const auto revision=timerRevision_;
        resetDialogue(); // Consume once, retaining the displayed position mapping.
        switch(kind) {
        case Pending::Note: {
            const auto size=sys.state.notes.size();const auto reply=api.addNote(a);
            if(sys.state.notes.size()>size) {contextId_=sys.state.notes.back().id;contextNote_=true;}
            return reply;
        }
        case Pending::Contact:return api.addContact(a,b);
        case Pending::Event:return api.addEvent(a,date,reminder);
        case Pending::Timer:
            if(sys.timerRevision()!=revision) return "O timer mudou. Consulte novamente antes de iniciar outro.";
            return api.startTimer(ms);
        case Pending::EditNote:case Pending::RenameNote:
            contextId_=oldNote.id;contextNote_=true;return api.editNote(oldNote,a,kind==Pending::RenameNote);
        case Pending::DeleteNote:return api.deleteNote(oldNote);
        case Pending::EditEvent:contextId_=oldEvent.id;contextNote_=false;return api.editEvent(oldEvent,newEvent);
        case Pending::DeleteEvent:return api.deleteEvent(oldEvent);
        case Pending::ChangeTimer:return api.changeTimer(ms,revision);
        case Pending::CancelTimer:return api.cancelTimer(revision);
        default:break;
        }
    }
    if(key=="sim"||key=="confirmar"||key=="confirmo") return "Não há pedido aguardando confirmação.";
    auto help=s;
    if(!help.empty()&&(help.back()=='?'||help.back()=='.'||help.back()=='!')) help=trim(help.substr(0,help.size()-1));
    if(help=="ajuda"||help=="/ajuda"||help=="help"||help=="comandos"||help=="quais comandos"||
       help=="o que voce faz"||help=="o que vc faz"||help=="o que voce pode fazer"||help=="o que voce consegue fazer") {
        cursor_=Cursor::Help;offset_=0;return readPage(sys);
    }
    if(starts(help,"ajuda ")) {
        const auto topic=trim(help.substr(6));size_t page=helpPages;
        if(topic=="nota"||topic=="notas") page=1;
        if(topic=="agenda"||topic=="eventos"||topic=="lembretes") page=2;
        if(topic=="timer"||topic=="temporizador") page=3;
        if(topic=="contatos"||topic=="calculos"||topic=="calculadora"||topic=="sistema"||topic=="bateria"||topic=="hora") page=4;
        if(topic=="calendario"||topic=="datas"||topic=="dias"||topic=="semana") page=5;
        if(page==helpPages) return "Temas disponíveis: ajuda notas, ajuda agenda, ajuda calendario, ajuda timer e ajuda contatos. Digite ajuda para ver o resumo.";
        cursor_=Cursor::Help;offset_=page;return readPage(sys);
    }
    const auto recordReply=records(sys,raw,s);if(!recordReply.empty()) return recordReply;
    std::string payload;
    if(noteRequest(raw,payload)) {
        if(payload.empty()) return "Informe o texto: nota: trocar óleo do carro";
        pending_=Pending::Note;first_=payload;
        return "Criar nota com o texto:\n"+payload+"\nDigite sim ou cancelar.";
    }
    // Free-form notes may contain negations. All other action grammars reject
    // qualifiers/compound requests rather than silently dropping instructions.
    if(word(s,"nao")||word(s,"sem")||s.find(';')!=std::string::npos||s.find('\n')!=std::string::npos)
        return "Esse pedido tem uma restrição que não consigo aplicar com segurança. Nada foi alterado.";
    const bool hasClock=sys.clockValid();
    const auto calendar=calendarReply(key,hasClock,hasClock?fromUtc(sys.utc(),sys.state.preferences.timezoneMinutes):CivilTime());
    if(!calendar.empty()) return calendar;
    bool cancelTimer=false;
    if(timerControl(key,cancelTimer,payload)) {
        if(cancelTimer&&!payload.empty()) return "Para cancelar a contagem atual, use cancela o timer, sem outras instruções.";
        if(!sys.state.timerMs&&!sys.timerAlertActive()) return cancelTimer?"Não há timer para cancelar.":"Não há timer para alterar. Use timer de 5 minutos para criar.";
        if(cancelTimer) {
            pending_=Pending::CancelTimer;timerRevision_=sys.timerRevision();
            return api.timer()+"\nCancelar e zerar esse timer?\nDigite sim ou cancelar.";
        }
        uint64_t ms=0;
        if(!duration(payload,ms)) return "Use altera o timer para 5 minutos. O valor substitui o tempo restante.";
        if(!sys.state.timerMs) return "O timer já terminou. Use timer com a duração para criar outro.";
        duration_=ms;timerRevision_=sys.timerRevision();pending_=Pending::ChangeTimer;
        return api.timer()+"\nDefinir tempo restante para "+secondsText(ms)+
            (sys.state.timerRunning?".":". Mantendo pausado.")+"\nDigite sim ou cancelar.";
    }
    for(const char *q:{"timer","temporizador","status do timer","le timer","ler o timer","mostra o timer","mostra o temporizador",
        "quanto tempo falta no timer","quanto falta para o timer acabar","quanto tempo restou no temporizador",
        "quanto tempo ainda tem no temporizador","como esta o tempo do timer","quanto falta no timer","tempo restante do timer"})
        if(key==q) return api.timer();
    for(const char *p:{"salva o contato da ","salva o contato de ","salva o contato ","salva contato ",
        "cria um contato ","cria o contato ","cria contato ","adiciona um contato ","adiciona o contato ","adiciona contato ","add contato ","contato "}) {
        if(prefixPayload(raw,s,p,payload)) {
            auto colon=payload.find(':');size_t skip=1;
            if(colon==std::string::npos) for(const char *sep:{" com o telefone "," com o numero "," telefone "," numero "}) {
                const auto at=fold(payload).find(sep);if(at==std::string::npos) continue;
                colon=rawOffset(payload,at);skip=rawOffset(payload.substr(colon),std::char_traits<char>::length(sep));break;
            }
            if(colon==std::string::npos) return "Use contato Nome: telefone. Exemplo: contato Oficina: 11 99999-8888";
            auto name=trim(payload.substr(0,colon)),phone=trim(payload.substr(colon+skip));
            if(name.empty()||!phoneValid(phone)) return "Confira o nome e o telefone (8 a 15 dígitos).";
            pending_=Pending::Contact;first_=name;second_=phone;
            return "Criar contato:\n"+name+"\n"+phone+"\nDigite sim ou cancelar.";
        }
    }
    // Explicit date; ask for missing time instead of silently choosing midnight.
    bool reminder=false;
    const bool event=eventRequest(raw,s,payload,reminder);
    if(event) {
        if(!sys.clockValid()) return "Ajuste a data e a hora em Relógio antes de cadastrar na agenda.";
        CivilTime date;std::string title;bool hasTime=false;
        if(!eventDetails(payload,date,title,hasTime,true,fromUtc(sys.utc(),sys.state.preferences.timezoneMinutes)))
            return "Use evento Título DD/MM/AAAA HH:MM ou crie um evento para dia 5 de novembro de 2026 para comprar pão.";
        if(!sys.clockValid()) return "Ajuste a data e a hora em Relógio antes de cadastrar na agenda.";
        if(!hasTime) {
            if(dateKey(date)<dateKey(fromUtc(sys.utc(),sys.state.preferences.timezoneMinutes))) return "Essa data já passou. Informe uma data futura.";
            first_=title;reminder_=reminder;date_=toUtc(date,0);pending_=Pending::EventTime;
            return "Criar "+std::string(reminder?"lembrete: ":"evento: ")+title+"\n"+formatCivil(date_,0,false)+
                "\nQual horário? Responda 09:00, 9h ou às 9 da manhã; cancelar descarta.";
        }
        date_=toUtc(date,sys.state.preferences.timezoneMinutes);
        if(date_<=sys.utc()) {clear();return "Essa data já passou. Informe uma data futura.";}
        first_=title;reminder_=reminder;pending_=Pending::Event;
        return eventPreview(title,date_,sys.state.preferences.timezoneMinutes,reminder);
    }
    if(timerRequest(key,payload)) {
            uint64_t ms=0;
            if(!duration(payload,ms)) return "Informe a duração e a unidade: timer de 5 segundos, timer 12 min ou timer meia hora.";
            duration_=ms;timerRevision_=sys.timerRevision();pending_=Pending::Timer;
            return std::string(sys.state.timerMs?"Substituir o timer atual por ":"Iniciar timer de ")+secondsText(ms)+"?\nDigite sim ou cancelar.";
    }
    for(const char *p:{"qual o telefone de ","qual o telefone da ","qual o telefone do ","qual o celular da ","qual o celular do ","qual o celular de ","qual o cel do ","qual o contato de ","qual o contato da ","qual o contato do ",
        "qual e o telefone de ","qual e o telefone da ","qual e o telefone do ","me da o numero de ","me da o numero da ","me da o numero do ","me passa o telefone de ","me passa o telefone da ","me passa o telefone do ","me passa o contato de ","me passa o contato da ","me passa o contato do ","me diga o telefone de ",
        "mostra o contato de ","mostra o contato da ","mostra o contato do ","mostra o telefone de ","mostra o telefone da ","mostra o telefone do ",
        "qual o numero de celular de ","qual o numero de celular da ","qual o numero de celular do ","qual o numero de telefone de ","qual o numero do contato ",
        "numero de ","numero da ","numero do ","num do ","num da ","num de ","telefone de ","telefone da ","telefone do ","celular de ","celular da ","celular do "}) {
        if(prefixPayload(raw,s,p,payload)) {
            const auto plain=commandKey(payload,false);
            for(const auto &c:sys.state.contacts) if(fold(c.name)==plain) return api.contact(payload.substr(0,rawOffset(payload,plain.size())));
            const auto clean=commandKey(payload);return api.contact(trim(payload.substr(0,rawOffset(payload,clean.size()))));
        }
    }
    int day=0;bool next=false,reminders=false;
    if(agendaQuery(key,day,next,reminders)) {
            const auto ids=api.agendaItems(day,next,reminders);
            if(!sys.clockValid()) return api.agenda(day,next,reminders);
            rememberPositions(sys,false,ids);contextId_=0;offset_=0;cursor_=Cursor::Events;
            if(ids.empty()) {cursor_=Cursor::None;return api.agenda(day,next,reminders);}
            return readPage(sys);
    }
    std::string answer;if(calculate(s,answer)) return answer;
    return "";
}
}
