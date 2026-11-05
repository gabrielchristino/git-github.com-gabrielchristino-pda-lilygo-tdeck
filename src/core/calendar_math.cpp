#include "pda/calendar_math.h"
#include <vector>
#include <cstdlib>
#include <cstdio>

namespace pda {
namespace {
using Words=std::vector<std::string>;
struct Date {int day=0,month=0,year=0;};
enum class Result { Ok, Invalid, Clock, Range };
const char *months[]={"janeiro","fevereiro","marco","abril","maio","junho","julho","agosto","setembro","outubro","novembro","dezembro"};
const char *weekdays[]={"domingo","segunda-feira","terça-feira","quarta-feira","quinta-feira","sexta-feira","sábado"};
bool number(const std::string &s,int maximum,int &out) {
    if(s.empty()||s.find_first_not_of("0123456789")!=std::string::npos||s.size()>5) return false;
    const auto value=strtol(s.c_str(),nullptr,10);if(value>maximum) return false;out=int(value);return true;
}
Words words(std::string s) {
    while(!s.empty()&&(s.back()=='?'||s.back()=='.'||s.back()=='!'||s.back()==' ')) s.pop_back();
    Words out;size_t i=0;
    while(i<s.size()) {
        const auto start=s.find_first_not_of(" \t",i);if(start==std::string::npos) break;
        const auto end=s.find_first_of(" \t",start);out.push_back(s.substr(start,end-start));
        if(end==std::string::npos) break;
        i=end+1;
    }
    return out;
}
std::string tail(const Words &w,size_t i) {
    std::string out;for(;i<w.size();++i) {if(!out.empty()) out+=' ';out+=w[i];}return out;
}
bool isMonth(const std::string &s,int &month) {
    for(int i=0;i<12;++i) if(s==months[i]) {month=i+1;return true;}
    return false;
}
bool parseDate(const Words &w,size_t &i,Date &date) {
    if(i<w.size()&&w[i]=="o") ++i;
    if(i<w.size()&&w[i]=="dia") ++i;
    if(i>=w.size()) return false;
    const auto &t=w[i++];const auto slash=t.find('/');
    if(slash!=std::string::npos) {
        const auto second=t.find('/',slash+1);
        if(!number(t.substr(0,slash),31,date.day)||
           !number(t.substr(slash+1,second==std::string::npos?second:second-slash-1),12,date.month)||!date.month) return false;
        if(second!=std::string::npos && (!number(t.substr(second+1),2099,date.year)||date.year<2000||t.substr(second+1).size()!=4)) return false;
    } else {
        if(!number(t,31,date.day)) return false;
        bool de=i<w.size()&&w[i]=="de";if(de) ++i;
        if(i<w.size()&&isMonth(w[i],date.month)) ++i;
        else if(de) return false;
        if(date.month) {
            de=i<w.size()&&w[i]=="de";if(de) ++i;
            if(i<w.size()&&w[i].size()==4&&number(w[i],2099,date.year)&&date.year>=2000) ++i;
            else if(de) return false;
            else date.year=0;
        }
    }
    if(date.day<1) return false;
    if(date.month&&date.day>daysInMonth(date.year?date.year:2000,date.month)) return false;
    return true;
}
CivilTime civil(const Date &s,int year,int month) {
    CivilTime d;d.year=s.year?s.year:year;d.month=s.month?s.month:month;d.day=s.day;return d;
}
// Earliest occurrence on/after the reference. Explicit years are never moved.
Result forward(const Date &s,const CivilTime &reference,bool hasReference,CivilTime &out) {
    if(s.year) {out=civil(s,s.year,s.month);return validCivil(out)?Result::Ok:Result::Invalid;}
    if(!hasReference) return Result::Clock;
    for(int y=reference.year;y<=2099;++y) {
        for(int m=1;m<=12;++m) {
            if(s.month&&m!=s.month) continue;
            const auto d=civil(s,y,m);
            if(validCivil(d)&&dateKey(d)>=dateKey(reference)) {out=d;return Result::Ok;}
        }
    }
    return Result::Range;
}
Result backward(const Date &s,const CivilTime &reference,CivilTime &out) {
    for(int y=reference.year;y>=2000;--y) {
        for(int m=12;m>=1;--m) {
            if(s.month&&m!=s.month) continue;
            const auto d=civil(s,y,m);
            if(validCivil(d)&&dateKey(d)<=dateKey(reference)) {out=d;return Result::Ok;}
        }
    }
    return Result::Range;
}
std::string error(Result r) {
    if(r==Result::Clock) return "Para inferir o mês ou o ano, ajuste a data e a hora em Relógio. Com uma data completa, calculo sem depender do relógio.";
    if(r==Result::Range) return "A data calculada está fora do intervalo de 2000 a 2099.";
    return "Data inválida. Confira o dia, o mês e o ano (2000 a 2099).";
}
std::string stamp(const CivilTime &d) {return formatCivil(toUtc(d,0),0,false);}
std::string describe(const CivilTime &d) {return stamp(d)+" ("+weekdays[weekDay(d)]+").";}
std::string unit(int count,bool business) {
    return count==1?(business?" dia útil":" dia corrido"):(business?" dias úteis":" dias corridos");
}
bool shift(const CivilTime &start,int count,CivilTime &out) {
    out=fromUtc(toUtc(start,0)+int64_t(count)*86400,0);return validCivil(out);
}
bool businessShift(CivilTime start,int count,CivilTime &out) {
    const int direction=count<0?-1:1;int left=count<0?-count:count;
    // Move off a weekend first; whole weeks then take constant time.
    while(left && (weekDay(start)==0||weekDay(start)==6)) {
        if(!shift(start,direction,start)) return false;
        if(weekDay(start)!=0&&weekDay(start)!=6) --left;
    }
    if(!shift(start,(left/5)*7*direction,start)) return false;
    left%=5;
    while(left) {
        if(!shift(start,direction,start)) return false;
        if(weekDay(start)!=0&&weekDay(start)!=6) --left;
    }
    out=start;return true;
}
int businessDays(CivilTime start,int days) {
    int result=(days/7)*5;
    // Only the weekday is needed after complete weeks; no date rollover work.
    const auto weekday=weekDay(start);
    for(int i=1;i<=days%7;++i) {const auto day=(weekday+i)%7;if(day!=0&&day!=6) ++result;}
    return result;
}
bool dateStart(const Words &w,size_t i) {
    if(i>=w.size()) return false;
    if(w[i]=="dia"||w[i].find('/')!=std::string::npos) return true;
    int month=0,day=0;if(!number(w[i],31,day)||!day) return false;
    return i+1<w.size()&&(w[i+1]=="de"||isMonth(w[i+1],month));
}
bool intervalSuffix(std::string suffix,bool &business) {
    business=false;
    for(const char *ending:{" uteis"," corridos"}) {
        const std::string e=ending;
        if(suffix.size()>=e.size()&&suffix.compare(suffix.size()-e.size(),e.size(),e)==0) {
            business=e==" uteis";suffix.erase(suffix.size()-e.size());break;
        }
    }
    return suffix.empty()||suffix=="sao quantos dias"||suffix=="quantos dias"||suffix=="tem quantos dias"||suffix=="da quantos dias";
}
}
std::string calendarReply(const std::string &input,bool hasToday,CivilTime today) {
    auto w=words(input);if(w.empty()) return "";
    today.hour=today.minute=today.second=0;
    size_t i=0;bool weekday=false;
    const std::string prefixes[]={"quando cai ","que dia cai ","em qual dia cai ","em qual dia da semana cai ","em que dia cai ","em que dia da semana cai ","que dia da semana cai ","qual dia da semana cai ","qual o dia da semana de ","que dia da semana e ","qual e o dia da semana de ","dia da semana de "};
    for(const auto &prefix:prefixes) if(input.compare(0,prefix.size(),prefix)==0) {i=words(prefix).size();weekday=true;break;}
    if(weekday) {
        Date date;CivilTime resolved;
        if(!parseDate(w,i,date)||i!=w.size()) return "Use quando cai dia 8 de novembro de 2026, quando cai dia 8 de novembro ou quando cai dia 8.";
        const auto r=forward(date,today,hasToday,resolved);if(r!=Result::Ok) return error(r);
        return stamp(resolved)+": "+weekdays[weekDay(resolved)]+".";
    }
    bool interval=w[0]=="do"||w[0]=="de"||w[0]=="entre";
    if(w.size()>=4&&w[0]=="quantos"&&w[1]=="dias") {
        i=2;
        if(w[i]=="sao"||w[i]=="tem"||w[i]=="ha") ++i;
        if(i<w.size()&&(w[i]=="entre"||w[i]=="de")) interval=true;
    }
    if(interval) {
        const bool between=w[i]=="entre";++i;
        Date first,last;CivilTime start,end;
        if(!parseDate(w,i,first)||i>=w.size()||(w[i]!="ate"&&w[i]!="a"&&!(between&&w[i]=="e")))
            return "Use do dia 1 de novembro até 25 de dezembro são quantos dias.";
        ++i;bool business=false;
        if(!parseDate(w,i,last)||!intervalSuffix(tail(w,i),business)) return "Confira as duas datas. Exemplo: de 01/11/2026 até 25/12/2026 são quantos dias.";
        Result r=Result::Ok;
        if(!first.year&&last.year) {
            r=forward(last,today,hasToday,end);if(r==Result::Ok) r=backward(first,end,start);
        } else {
            if(first.year) r=forward(first,today,hasToday,start);
            else if(!hasToday) r=Result::Clock;
            else {
                start=civil(first,today.year,today.month);
                if(!validCivil(start)) r=forward(first,today,true,start);
            }
            if(r==Result::Ok) r=forward(last,start,true,end);
        }
        if(r!=Result::Ok) return error(r);
        const int days=int((toUtc(end,0)-toUtc(start,0))/86400);
        if(days<0) return "A data final é anterior à inicial. Informe o intervalo em ordem.";
        const int count=business?businessDays(start,days):days;
        return "De "+stamp(start)+" até "+stamp(end)+": "+std::to_string(count)+unit(count,business)+".\nSem contar o dia inicial."+(business?" Segunda a sexta, sem feriados.":"");
    }
    i=0;if(w.size()>2&&w[0]=="quanto"&&w[1]=="e") i=2;
    bool hasDayWord=false;for(const auto &t:w) if(t=="dia"||t=="dias") hasDayWord=true;
    if(!hasDayWord) return ""; // Preserve ordinary divisions such as 12/2 + 5.
    if(!dateStart(w,i)) return "";
    Date date;const auto begin=i;
    if(!parseDate(w,i,date)) return "Data inválida. Use dia 8 de novembro de 2026 mais 5 dias corridos.";
    if(i>=w.size()||(w[i]!="mais"&&w[i]!="menos"&&w[i]!="+"&&w[i]!="-")) {
        // Date-like content inside a different capability is not executed here.
        return begin==0&&w[0]=="dia"?"Use quando cai dia 8 ou dia 8 mais 5 dias corridos.":"";
    }
    const bool minus=w[i]=="menos"||w[i]=="-";++i;int count=0;
    if(i>=w.size()||!number(w[i++],36525,count)||i>=w.size()||(w[i]!="dia"&&w[i]!="dias"))
        return "Informe um número inteiro de 0 a 36525 e a unidade dias.";
    ++i;bool business=false;
    if(i<w.size()) {business=w[i]=="uteis"||w[i]=="util";if(!business&&w[i]!="corridos"&&w[i]!="corrido") return "Use dias corridos ou dias úteis.";++i;}
    if(i!=w.size()) return "Envie só o cálculo de calendário, sem outras instruções.";
    CivilTime start,result;const auto r=forward(date,today,hasToday,start);if(r!=Result::Ok) return error(r);
    const auto signedCount=minus?-count:count;
    if(!(business?businessShift(start,signedCount,result):shift(start,signedCount,result))) return error(Result::Range);
    return stamp(start)+(minus?" - ":" + ")+std::to_string(count)+unit(count,business)+" = "+describe(result)+
        (business?"\nSegunda a sexta, sem feriados. Não conta o dia inicial.":"");
}
}
