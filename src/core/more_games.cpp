#include "pda/more_games.h"
#include "pda/model.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <set>

namespace pda { namespace games {
namespace {
uint32_t rnd(Save &g) { uint32_t n=g.seed ? g.seed : 1; n^=n<<13; n^=n>>17; n^=n<<5; return g.seed=n; }
char hex(unsigned n) { return "0123456789abcdef"[n&15]; }
unsigned nib(char c) { return c<='9' ? unsigned(c-'0') : unsigned(c-'a'+10); }
std::string byte(unsigned n) { return std::string(1,hex(n>>4))+hex(n); }
unsigned number(const std::string &s,unsigned p=0) { return nib(s[p])*16+nib(s[p+1]); }
bool hexadecimal(const std::string &s) { return s.find_first_not_of("0123456789abcdef")==std::string::npos; }
std::string uppercase(std::string s) { s=lowerAscii(s); for(char &c:s) if(c>='a'&&c<='z') c=char(c-'a'+'A'); return s; }
int sign(int n) { return (n>0)-(n<0); }
bool cursor(Save &g,int dx,int dy,int width,int height) {
    int x=std::max(0,std::min(width-1,int(g.cursor%width)+sign(dx)));
    int y=std::max(0,std::min(height-1,int(g.cursor/width)+sign(dy)));
    if(y*width+x==g.cursor) return false;
    g.cursor=uint8_t(y*width+x); return true;
}
const unsigned ships[]={4,3,3,2};
bool fits(const std::string &board,unsigned base,unsigned pos,unsigned length,bool vertical) {
    if((vertical ? pos/8+length : pos%8+length)>8) return false;
    for(unsigned i=0;i<length;++i) if(board[base+pos+i*(vertical?8:1)]!='.') return false;
    return true;
}
void fleet(Save &g,unsigned base) {
    g.board.replace(base,64,64,'.');
    for(unsigned ship=0;ship<4;++ship) {
        bool placed=false;
        for(unsigned tries=0;tries<4096;++tries) {
            const unsigned p=rnd(g)%64; const bool vertical=(rnd(g)&1)!=0;
            if(!fits(g.board,base,p,ships[ship],vertical)) continue;
            for(unsigned i=0;i<ships[ship];++i) g.board[base+p+i*(vertical?8:1)]=char('A'+ship);
            placed=true; break;
        }
        if(!placed) { // Bounded fallback: four non-overlapping horizontal ships.
            g.board.replace(base,64,64,'.');
            for(unsigned s=0;s<4;++s) for(unsigned i=0;i<ships[s];++i) g.board[base+s*16+i]=char('A'+s);
            return;
        }
    }
}
bool fleetValid(const std::string &board,unsigned base,unsigned placed) {
    for(unsigned s=0;s<4;++s) {
        std::vector<unsigned> positions;
        for(unsigned p=0;p<64;++p) if(board[base+p]==char('A'+s)) positions.push_back(p);
        if(positions.size()!=(s<placed?ships[s]:0)) return false;
        if(positions.empty()) continue;
        bool horizontal=true,vertical=true;
        for(unsigned i=1;i<positions.size();++i) {
            horizontal=horizontal && positions[i]==positions[0]+i && positions[i]/8==positions[0]/8;
            vertical=vertical && positions[i]==positions[0]+8*i;
        }
        if(!horizontal&&!vertical) return false;
    }
    return true;
}
std::vector<unsigned> adjacent(unsigned p,unsigned width,unsigned height) {
    std::vector<unsigned> v;
    if(p%width) v.push_back(p-1);
    if(p%width+1<width) v.push_back(p+1);
    if(p>=width) v.push_back(p-width);
    if(p+width<width*height) v.push_back(p+width);
    return v;
}
bool navalFire(Save &g) {
    const unsigned p=64+g.cursor;
    if(g.status||g.level!=4||g.marks[p]!='?') return false;
    g.marks[p]=g.board[p]=='.'?'o':'x'; ++g.moves;
    if(g.marks[p]=='x') ++g.score;
    if(g.score==12) { g.status=1; ++g.best; return true; }
    std::vector<unsigned> targets,untried;
    for(unsigned i=0;i<64;++i) {
        if(g.marks[i]=='?') untried.push_back(i);
        if(g.marks[i]=='x') for(unsigned q:adjacent(i,8,8)) if(g.marks[q]=='?') targets.push_back(q);
    }
    const auto &choices=targets.empty()?untried:targets;
    if(choices.empty()) return true;
    const unsigned q=choices[rnd(g)%choices.size()]; g.undoMoves=q+1;
    g.marks[q]=g.board[q]=='.'?'o':'x';
    if(g.marks[q]=='x') ++g.undoScore;
    if(g.undoScore==12) g.status=2;
    return true;
}
void food(Save &g) {
    const auto body=snakeBody(g); std::vector<unsigned> empty;
    for(unsigned i=0;i<128;++i) if(std::find(body.begin(),body.end(),i)==body.end()) empty.push_back(i);
    if(empty.empty()) { g.marks.clear(); g.status=1; g.undoStatus=1; }
    else g.marks=byte(empty[rnd(g)%empty.size()]);
}
std::pair<unsigned,unsigned> feedback(const std::string &secret,const std::string &attempt) {
    unsigned exact=0,other=0,need[6]={},have[6]={};
    for(unsigned i=0;i<4;++i) if(secret[i]==attempt[i]) ++exact; else { ++need[secret[i]-'1']; ++have[attempt[i]-'1']; }
    for(unsigned i=0;i<6;++i) other+=std::min(need[i],have[i]);
    return {exact,other};
}
bool codeSubmit(Save &g) {
    if(g.status) return false;
    const auto result=feedback(g.board,g.undoBoard);
    g.marks+=g.undoBoard+char('0'+result.first)+char('0'+result.second); ++g.moves; g.score=result.first;
    g.status=result.first==4?1:g.moves==10?2:0;
    if(g.status==1) ++g.best;
    return true;
}
bool placeWord(std::string &board,const std::string &word,unsigned start,int dx,int dy) {
    const int width=board.size()==80?10:16,height=int(board.size())/width;
    const int ex=int(start%width)+dx*int(word.size()-1),ey=int(start/width)+dy*int(word.size()-1);
    if(ex<0||ex>=width||ey<0||ey>=height) return false;
    for(unsigned i=0;i<word.size();++i) {
        const unsigned p=unsigned(int(start)+(dx+width*dy)*int(i));
        if(board[p]!='.'&&board[p]!=word[i]) return false;
    }
    for(unsigned i=0;i<word.size();++i) board[unsigned(int(start)+(dx+width*dy)*int(i))]=word[i];
    return true;
}
bool containsWord(const std::string &board,const std::string &word) {
    const int width=board.size()==80?10:16,height=int(board.size())/width;
    for(unsigned p=0;p<board.size();++p) for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        if(!dx&&!dy) continue;
        const int x=int(p%width)+dx*int(word.size()-1),y=int(p/width)+dy*int(word.size()-1);
        if(x<0||x>=width||y<0||y>=height) continue;
        bool same=true;
        for(unsigned i=0;i<word.size();++i) if(board[unsigned(int(p)+(dx+width*dy)*int(i))]!=word[i]) { same=false; break; }
        if(same) return true;
    }
    return false;
}
void wordGrid(Save &g) {
    std::vector<unsigned> pool;
    for(unsigned i=unsigned(g.level)*16;i<(unsigned(g.level)+1)*16;++i) if(uppercase(wordAt(i)).size()<=13) pool.push_back(i);
    g.marks.clear();
    for(unsigned i=0;i<10;++i) { const unsigned q=i+rnd(g)%(pool.size()-i); std::swap(pool[i],pool[q]); g.marks+=byte(pool[i]>>8)+byte(pool[i]); }
    g.board.assign(208,'.'); bool complete=true;
    for(unsigned i=0;i<10;++i) {
        const auto word=uppercase(searchWord(g,i)); bool placed=false;
        for(unsigned tries=0;tries<600;++tries) {
            const unsigned p=rnd(g)%208; const int dx=int(rnd(g)%3)-1,dy=int(rnd(g)%3)-1;
            if((dx||dy)&&placeWord(g.board,word,p,dx,dy)) { placed=true; break; }
        }
        if(!placed) { complete=false; break; }
    }
    if(!complete) { g.board.assign(208,'.'); for(unsigned i=0;i<10;++i) placeWord(g.board,uppercase(searchWord(g,i)),i*16,1,0); }
    for(char &c:g.board) if(c=='.') c=char('A'+rnd(g)%26);
    g.undoBoard.assign(208,'0');
}
bool wordSelect(Save &g) {
    if(g.status) return false;
    if(!g.undoStatus) { g.undoCursor=g.cursor; g.undoStatus=1; g.undoScore=0; return true; }
    const auto path=searchPath(g,g.undoCursor,g.cursor); std::string word;
    for(unsigned p:path) word+=g.board[p];
    g.undoStatus=0; g.undoScore=3; ++g.moves;
    for(unsigned i=0;i<searchCount(g);++i) {
        auto target=uppercase(searchWord(g,i)); auto reverse=target; std::reverse(reverse.begin(),reverse.end());
        if(word!=target&&word!=reverse) continue;
        if(g.score&(1u<<i)) { g.undoScore=2; return true; }
        g.score|=1u<<i; g.undoScore=1;
        for(unsigned p:path) g.undoBoard[p]='1';
        if(g.score==((1u<<searchCount(g))-1)) { g.status=1; ++g.best; }
        break;
    }
    return true;
}
// Dungeon uses the existing save fields: health, healing bottles, shield turns,
// two inventory digits (bombs/shields), and a bounded last-event identifier.
std::vector<int> distances(const std::string &map,unsigned origin) {
    const unsigned width=map.size()==99?11:19, height=unsigned(map.size())/width;
    std::vector<int> d(map.size(),-1); std::vector<unsigned> queue={origin}; d[origin]=0;
    for(size_t i=0;i<queue.size();++i) for(unsigned q:adjacent(queue[i],width,height)) if(map[q]!='#'&&d[q]<0) { d[q]=d[queue[i]]+1; queue.push_back(q); }
    return d;
}
void reveal(Save &g) {
    const unsigned width=dungeonWidth(g),height=dungeonHeight(g);
    const auto d=distances(g.board,g.cursor);
    for(unsigned p=0;p<g.board.size();++p) if(d[p]>=0&&d[p]<=3) {
        g.marks[p]='1';
        for(unsigned q:adjacent(p,width,height)) if(g.board[q]=='#') g.marks[q]='1';
    }
}
void dungeonFloor(Save &g,unsigned floor) {
    const unsigned width=19,height=13,size=width*height,start=width+1;
    g.level=uint16_t(floor); g.board.assign(size,'#'); g.marks.assign(size,'0'); g.cursor=uint8_t(start); g.board[start]='.';
    g.undoSeed=0; g.undoStatus=0;
    std::vector<unsigned> stack={start};
    while(!stack.empty()) {
        const unsigned p=stack.back(); std::vector<unsigned> options;
        for(int step:{-int(width)*2,-2,2,int(width)*2}) {
            const int q=int(p)+step;
            if(q>0&&q<int(size)&&q%int(width)>0&&q%int(width)<int(width)-1&&q/int(width)>0&&q/int(width)<int(height)-1&&
                std::abs(q%int(width)-int(p%width))+std::abs(q/int(width)-int(p/width))==2&&g.board[unsigned(q)]=='#') options.push_back(unsigned(q));
        }
        if(options.empty()) { stack.pop_back(); continue; }
        const unsigned q=options[rnd(g)%options.size()]; g.board[(p+q)/2]='.'; g.board[q]='.'; stack.push_back(q);
    }
    for(unsigned p=start;p<size-width-1;++p) if(p%width>0&&p%width<width-1&&g.board[p]=='#'&&rnd(g)%5==0) {
        if((g.board[p-1]!='#'&&g.board[p+1]!='#')||(g.board[p-width]!='#'&&g.board[p+width]!='#')) g.board[p]='.';
    }
    const auto d=distances(g.board,start); const unsigned exit=unsigned(std::max_element(d.begin(),d.end())-d.begin()); g.board[exit]='>';
    std::vector<unsigned> empty;
    for(unsigned p=0;p<size;++p) if(g.board[p]=='.'&&d[p]>=5) empty.push_back(p);
    unsigned n=0;
    auto item=[&](char c) { if(n>=empty.size()) return; const unsigned q=n+rnd(g)%(empty.size()-n); std::swap(empty[n],empty[q]); g.board[empty[n++]]=c; };
    for(unsigned i=0;i<3+floor;++i) item('E');
    for(unsigned i=0;i<2+floor;++i) item('B');
    for(unsigned i=0;i<1+floor;++i) item('O');
    for(unsigned i=0;i<5;++i) item('T');
    item('H'); item('H'); item('H'); item('P'); item('P'); item('Q'); item('X'); reveal(g);
}
void enemies(Save &g,unsigned defense=0) {
    std::vector<unsigned> positions;
    for(unsigned p=0;p<g.board.size();++p) if(dungeonEnemy(g.board[p])) positions.push_back(p);
    const auto d=distances(g.board,g.cursor);
    const unsigned armor=defense+(g.undoSeed?1:0);
    for(unsigned p:positions) {
        if(!g.undoScore) break;
        const char type=g.board[p];
        // Ogres act every other turn; bats wander instead of tracking through walls.
        if((type=='O'||type=='o')&&g.moves%2) continue;
        if(d[p]==1) {
            const unsigned strength=(type=='O'||type=='o')?3:1, damage=strength>armor?strength-armor:0;
            g.undoScore-=std::min(g.undoScore,damage);
            if(damage) g.undoStatus=10;
            continue;
        }
        if(d[p]<2||d[p]>8) continue;
        const auto options=adjacent(p,dungeonWidth(g),dungeonHeight(g));
        if(type=='B') {
            const unsigned q=options[rnd(g)%options.size()];
            if(q!=g.cursor&&g.board[q]=='.') { g.board[q]=type; g.board[p]='.'; }
        } else for(unsigned q:options) if(q!=g.cursor&&g.board[q]=='.'&&d[q]>=0&&d[q]<d[p]) { g.board[q]=type; g.board[p]='.'; break; }
    }
    if(g.undoSeed) --g.undoSeed;
    if(!g.undoScore) g.status=2;
    g.best=std::max(g.best,g.score); reveal(g);
}
bool dungeonMove(Save &g,int dx,int dy) {
    if(g.status) return false;
    const unsigned width=dungeonWidth(g);
    const int x=int(g.cursor%width)+(dy?0:sign(dx)),y=int(g.cursor/width)+(dy?sign(dy):0);
    if(x<0||x>=int(width)||y<0||y>=int(dungeonHeight(g))) return false;
    const unsigned p=unsigned(y)*width+unsigned(x);
    if(p==g.cursor||g.board[p]=='#') return false;
    ++g.moves; g.undoStatus=0; char &cell=g.board[p];
    if(dungeonEnemy(cell)) {
        if(cell=='O') { cell='o'; g.undoStatus=2; }
        else { g.score+=cell=='o'?6:cell=='B'?3:2; cell='.'; g.undoStatus=1; }
    } else {
        g.cursor=uint8_t(p);
        if(cell=='>') {
            g.score+=25; g.best=std::max(g.best,g.score);
            if(g.level==2) { g.status=1; reveal(g); } else dungeonFloor(g,g.level+1);
            return true;
        }
        if(cell=='T') { g.score+=10; g.undoStatus=3; }
        if(cell=='H') { g.undoScore=std::min(20u,g.undoScore+4); g.undoStatus=4; }
        if(cell=='P') { g.undoMoves=std::min(9u,g.undoMoves+1); g.undoStatus=5; }
        if(cell=='Q'||cell=='X') {
            if(g.undoBoard.empty()) g.undoBoard="00";
            const unsigned i=cell=='X'?0:1;
            if(g.undoBoard[i]<'9') ++g.undoBoard[i];
            g.undoStatus=cell=='X'?6:7;
        }
        cell='.';
    }
    enemies(g); return true;
}
}
unsigned dungeonWidth(const Save &g) { return g.board.size()==99?11:19; }
unsigned dungeonHeight(const Save &g) { return unsigned(g.board.size())/dungeonWidth(g); }
bool dungeonEnemy(char c) { return c=='E'||c=='B'||c=='O'||c=='o'; }
unsigned dungeonSupply(const Save &g,unsigned item) { return item<2&&g.undoBoard.size()==2?unsigned(g.undoBoard[item]-'0'):0; }
const char *dungeonMessage(const Save &g) {
    static const char *messages[]={"WASD: andar/atacar | ?: ajuda","Inimigo derrotado!","Ogro ferido!","Tesouro: +10 pontos","Fonte: +4 de vida","Poção de cura recolhida","Bomba recolhida","Poção de escudo recolhida","Cura: +6 de vida","Escudo protege por 6 turnos","Você sofreu um ataque!","Bomba lançada!","Em defesa neste turno"};
    return g.status==1?"Você escapou dos três andares!":g.status==2?"Fim da aventura. N: nova":messages[g.undoStatus<13?g.undoStatus:0];
}
bool dungeonAction(Save &g,char key) {
    if(!g.started||g.status) return false;
    if(key>='A'&&key<='Z') key=char(key-'A'+'a');
    if(key=='h') {
        if(!g.undoMoves||g.undoScore>=20) return false;
        --g.undoMoves; g.undoScore=std::min(20u,g.undoScore+6); g.undoStatus=8;
    } else if(key=='q') {
        if(!dungeonSupply(g,1)||g.undoSeed) return false;
        --g.undoBoard[1]; g.undoSeed=6; g.undoStatus=9;
    } else if(key=='x') {
        if(!dungeonSupply(g,0)) return false;
        --g.undoBoard[0]; g.undoStatus=11;
        const auto d=distances(g.board,g.cursor);
        for(unsigned p=0;p<g.board.size();++p) if(d[p]>=1&&d[p]<=2&&dungeonEnemy(g.board[p])) {
            g.score+=g.board[p]=='O'||g.board[p]=='o'?6:g.board[p]=='B'?3:2; g.board[p]='.';
        }
    } else if(key=='f'||key==' ') g.undoStatus=12;
    else return false;
    ++g.moves; enemies(g,key=='f'||key==' '?2:0); return true;
}
bool extra(Kind k) { return unsigned(k)>=unsigned(Kind::Naval)&&unsigned(k)<count; }
const char *extraName(Kind k) {
    switch(k) { case Kind::Naval:return "Batalha Naval"; case Kind::Snake:return "Cobrinha"; case Kind::Code:return "Senha Secreta"; case Kind::WordSearch:return "Caça-palavras"; case Kind::Dungeon:return "Masmorra"; default:return "Jogos"; }
}
const char *extraHelp(Kind k) {
    switch(k) {
    case Kind::Naval:return "Frota 4/3/3/2. WASD e Enter posicionam; espaço gira; R sorteia. Depois, Enter dispara. Esquerda: você; direita: PDA. Acerte 12 casas. M: jogos; N: nova.";
    case Kind::Snake:return "WASD dirige a cobra. Coma a fruta e evite paredes e o próprio corpo. Enter pausa/continua. M volta aos jogos e pausa. Ao reabrir, começa pausada. N pede nova partida.";
    case Kind::Code:return "Descubra 4 números de 1 a 6, com repetições, em 10 tentativas. WASD escolhe/altera; números digitam; Enter confirma. = posição certa, ~ número em outra posição. R limpa; M: jogos; N: nova.";
    case Kind::WordSearch:return "Ache as palavras do tema (10 nas novas grades 16x13), inclusive diagonais e invertidas. WASD move; Enter marca início e fim. Espaço cancela seleção. As palavras não exigem acentos na grade. M: jogos; N: nova.";
    case Kind::Dungeon:return "WASD: anda/ataca. g: guarda; m: morcego; o: ogro (2 golpes, lento). H: cura; Q: escudo; X: bomba (alcance 2). F/espaço: defende; Enter: espera. t: tesouro; +: fonte; p: poção; e: escudo; b: bomba; >: saída. M: jogos; N: nova.";
    default:return "";
    }
}
unsigned extraLevels(Kind k) { return k==Kind::WordSearch?levelCount(Kind::Hangman)/16:k==Kind::Dungeon?3:1; }
void extraReset(Kind k,Save &g,uint32_t seed,unsigned level) {
    const auto best=g.best; g=Save(); g.started=true; g.seed=seed?seed:1; g.best=best;
    if(k==Kind::Naval) { g.board.assign(128,'.'); g.marks.assign(128,'?'); fleet(g,64); }
    else if(k==Kind::Snake) { g.board=byte(67)+byte(66)+byte(65); g.undoStatus=1; food(g); }
    else if(k==Kind::Code) { for(unsigned i=0;i<4;++i) g.board+=char('1'+rnd(g)%6); g.undoBoard="1111"; }
    else if(k==Kind::WordSearch) { g.level=uint16_t(level%extraLevels(k)); wordGrid(g); }
    else { g.undoScore=20; g.undoMoves=2; g.undoBoard="11"; dungeonFloor(g,0); }
}
std::vector<unsigned> snakeBody(const Save &g) {
    std::vector<unsigned> result;
    for(unsigned p=0;p+1<g.board.size();p+=2) result.push_back(number(g.board,p));
    return result;
}
bool snakePause(Save &g) { if(!g.started||g.undoStatus) return false; g.undoStatus=1; return true; }
bool snakeStep(Save &g) {
    if(!g.started||g.status||g.undoStatus) return false;
    auto body=snakeBody(g); g.cursor=g.undoCursor; ++g.moves;
    const int dx=g.cursor==0?1:g.cursor==2?-1:0,dy=g.cursor==1?1:g.cursor==3?-1:0;
    const int x=int(body[0]%16)+dx,y=int(body[0]/16)+dy;
    const unsigned head=unsigned(y*16+x); const bool grow=!g.marks.empty()&&head==number(g.marks);
    const auto end=body.end()-(grow?0:1);
    if(x<0||x>=16||y<0||y>=8||std::find(body.begin(),end,head)!=end) { g.status=2; g.undoStatus=1; return true; }
    g.board=byte(head)+g.board;
    if(grow) { ++g.score; g.best=std::max(g.best,g.score); food(g); }
    else g.board.resize(g.board.size()-2);
    return true;
}
unsigned searchId(const Save &g,unsigned i) {
    if(i>=searchCount(g)||(g.marks.size()!=12&&g.marks.size()!=24&&g.marks.size()!=40)) return levelCount(Kind::Hangman);
    const unsigned stride=unsigned(g.marks.size())/searchCount(g),p=i*stride;
    return stride==2?number(g.marks,p):number(g.marks,p)*256+number(g.marks,p+2);
}
std::string searchWord(const Save &g,unsigned i) { return wordAt(searchId(g,i)); }
unsigned searchWidth(const Save &g) { return g.board.size()==80?10:16; }
unsigned searchHeight(const Save &g) { return unsigned(g.board.size())/searchWidth(g); }
unsigned searchCount(const Save &g) { return g.board.size()==80?6:10; }
std::vector<unsigned> searchPath(const Save &g,unsigned start,unsigned end) {
    if(start>=g.board.size()||end>=g.board.size()) return {};
    const int width=int(searchWidth(g));
    const int dx=int(end%width)-int(start%width),dy=int(end/width)-int(start/width);
    if(dx&&dy&&std::abs(dx)!=std::abs(dy)) return {};
    std::vector<unsigned> path; const unsigned length=unsigned(std::max(std::abs(dx),std::abs(dy)))+1;
    for(unsigned i=0;i<length;++i) path.push_back(unsigned(int(start)+(sign(dx)+width*sign(dy))*int(i)));
    return path;
}
std::vector<unsigned> searchPath(unsigned start,unsigned end) {
    Save legacy; legacy.board.resize(80); return searchPath(legacy,start,end);
}
bool extraMove(Kind k,Save &g,int dx,int dy) {
    if(!g.started||(!dx&&!dy)) return false;
    if(k==Kind::Naval) return cursor(g,dx,dy,8,8);
    if(k==Kind::Snake) {
        if(g.status) return false;
        const unsigned direction=dy?(dy>0?1:3):(dx>0?0:2);
        if(direction==(unsigned(g.cursor)+2)%4||g.undoCursor!=g.cursor) return false;
        const bool changed=g.undoCursor!=direction||g.undoStatus;
        g.undoCursor=uint8_t(direction); g.undoStatus=0; return changed;
    }
    if(k==Kind::Code) {
        if(g.status) return false;
        if(dx) { const unsigned p=unsigned(std::max(0,std::min(3,int(g.cursor)+sign(dx)))); if(p==g.cursor) return false; g.cursor=uint8_t(p); return true; }
        const int digit=g.undoBoard[g.cursor]-'1'; g.undoBoard[g.cursor]=char('1'+(digit+(dy<0?1:5))%6); return true;
    }
    if(k==Kind::WordSearch) return cursor(g,dx,dy,int(searchWidth(g)),int(searchHeight(g)));
    return dungeonMove(g,dx,dy);
}
bool extraActivate(Kind k,Save &g) {
    if(!g.started) return false;
    if(k==Kind::Naval) {
        if(g.level==4) return navalFire(g);
        if(!fits(g.board,0,g.cursor,ships[g.level],g.undoCursor!=0)) return false;
        for(unsigned i=0;i<ships[g.level];++i) g.board[g.cursor+i*(g.undoCursor?8:1)]=char('A'+g.level);
        ++g.level; if(g.level==4) g.cursor=0; return true;
    }
    if(k==Kind::Snake) { if(g.status) return false; g.undoStatus=uint8_t(!g.undoStatus); return true; }
    if(k==Kind::Code) return codeSubmit(g);
    if(k==Kind::WordSearch) return wordSelect(g);
    if(g.status) return false;
    ++g.moves; g.undoStatus=0; enemies(g); return true;
}
bool extraSpecial(Kind k,Save &g) {
    if(!g.started) return false;
    if(k==Kind::Naval) { if(g.level==4) return false; fleet(g,0); g.level=4; g.cursor=0; return true; }
    if(k==Kind::Snake) return extraActivate(k,g);
    if(k==Kind::Code) { if(g.status) return false; g.undoBoard="1111"; g.cursor=0; return true; }
    if(k==Kind::WordSearch) { if(!g.undoStatus) return false; g.undoStatus=0; return true; }
    return dungeonAction(g,'h');
}
bool extraSpace(Kind k,Save &g) {
    if(k==Kind::Dungeon) return dungeonAction(g,' ');
    if(k==Kind::Naval&&g.level<4) { g.undoCursor=uint8_t(!g.undoCursor); return true; }
    if(k==Kind::WordSearch) return extraSpecial(k,g);
    if(k==Kind::Code) return extraMove(k,g,0,-1);
    return extraActivate(k,g);
}
bool codeDigit(Save &g,unsigned n) { if(!g.started||g.status||n<1||n>6) return false; g.undoBoard[g.cursor]=char('0'+n); g.cursor=uint8_t((g.cursor+1)%4); return true; }
bool extraValid(Kind k,const Save &g) {
    if(!g.seed||g.status>2||g.moves>1000000000u||g.board.size()>512||g.marks.size()>512||g.undoBoard.size()>512||g.undoMarks.size()>512) return false;
    if(k==Kind::Naval) {
        if(g.board.size()!=128||g.marks.size()!=128||g.level>4||g.cursor>=64||g.undoCursor>1||g.undoMoves>64||g.undoScore>12||g.score>12||
            g.board.find_first_not_of(".ABCD")!=std::string::npos||g.marks.find_first_not_of("?ox")!=std::string::npos||!g.undoBoard.empty()||!g.undoMarks.empty()||
            !fleetValid(g.board,0,g.level)||!fleetValid(g.board,64,4)) return false;
        unsigned human=0,computer=0,shots=0;
        for(unsigned p=0;p<128;++p) { if(g.marks[p]=='x') { if(g.board[p]=='.') return false; if(p<64) ++computer; else ++human; }
            if(g.marks[p]=='o'&&g.board[p]!='.') return false;
            if(p>=64&&g.marks[p]!='?') ++shots;
        }
        if(human!=g.score||computer!=g.undoScore||shots!=g.moves) return false;
        return g.status==(human==12?1:computer==12?2:0);
    }
    if(k==Kind::Snake) {
        if(g.board.size()<6||g.board.size()>256||g.board.size()%2||!hexadecimal(g.board)||g.cursor>3||g.undoCursor>3||g.undoStatus>1||g.level||!g.undoBoard.empty()||!g.undoMarks.empty()) return false;
        const auto body=snakeBody(g); std::set<unsigned> unique(body.begin(),body.end());
        if(unique.size()!=body.size()||*unique.rbegin()>=128||g.score!=body.size()-3) return false;
        for(unsigned i=1;i<body.size();++i) if(std::abs(int(body[i]%16)-int(body[i-1]%16))+std::abs(int(body[i]/16)-int(body[i-1]/16))!=1) return false;
        if(body.size()==128) return g.status==1&&g.marks.empty()&&g.undoStatus==1;
        return g.status!=1&&g.marks.size()==2&&hexadecimal(g.marks)&&number(g.marks)<128&&!unique.count(number(g.marks))&&(g.status!=2||g.undoStatus==1);
    }
    if(k==Kind::Code) {
        if(g.board.size()!=4||g.undoBoard.size()!=4||g.board.find_first_not_of("123456")!=std::string::npos||g.undoBoard.find_first_not_of("123456")!=std::string::npos||
            g.cursor>=4||g.moves>10||g.marks.size()!=g.moves*6||g.level||!g.undoMarks.empty()) return false;
        unsigned exact=0;
        for(unsigned i=0;i<g.moves;++i) {
            const auto attempt=g.marks.substr(i*6,4); if(attempt.find_first_not_of("123456")!=std::string::npos) return false;
            const auto f=feedback(g.board,attempt); if(g.marks[i*6+4]!=char('0'+f.first)||g.marks[i*6+5]!=char('0'+f.second)||(i+1<g.moves&&f.first==4)) return false;
            exact=f.first;
        }
        return g.score==exact&&g.status==(exact==4?1:g.moves==10?2:0);
    }
    if(k==Kind::WordSearch) {
        if((g.board.size()!=80&&g.board.size()!=208)||g.board.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ")!=std::string::npos||(g.board.size()==80?(g.marks.size()!=12&&g.marks.size()!=24):g.marks.size()!=40)||!hexadecimal(g.marks)||
            g.undoBoard.size()!=g.board.size()||g.undoBoard.find_first_not_of("01")!=std::string::npos||!g.undoMarks.empty()||g.level>=extraLevels(k)||g.cursor>=g.board.size()||g.undoCursor>=g.board.size()||g.undoStatus>1||g.score>((1u<<searchCount(g))-1)||g.status!=(g.score==((1u<<searchCount(g))-1)?1:0)) return false;
        std::set<unsigned> ids;
        for(unsigned i=0;i<searchCount(g);++i) { const unsigned n=searchId(g,i); if(n>=levelCount(Kind::Hangman)||n/16!=g.level||!ids.insert(n).second||!containsWord(g.board,uppercase(wordAt(n)))) return false; }
        return true;
    }
    if(k!=Kind::Dungeon||(g.board.size()!=99&&g.board.size()!=247)||g.marks.size()!=g.board.size()||g.board.find_first_not_of("#.EBOoTH>PQX")!=std::string::npos||g.marks.find_first_not_of("01")!=std::string::npos||
        g.level>=3||g.cursor>=g.board.size()||g.board[g.cursor]=='#'||dungeonEnemy(g.board[g.cursor])||g.marks[g.cursor]!='1'||g.undoScore>20||g.undoMoves>9||g.undoSeed>6||g.undoStatus>12||g.undoCursor||
        (!g.undoBoard.empty()&&(g.undoBoard.size()!=2||g.undoBoard.find_first_not_of("0123456789")!=std::string::npos))||!g.undoMarks.empty()||std::count(g.board.begin(),g.board.end(),'>')!=1) return false;
    const unsigned width=dungeonWidth(g),height=dungeonHeight(g);
    for(unsigned p=0;p<g.board.size();++p) if((p%width==0||p%width==width-1||p/width==0||p/width==height-1)&&g.board[p]!='#') return false;
    const auto d=distances(g.board,g.cursor);
    for(unsigned p=0;p<g.board.size();++p) if(g.board[p]!='#'&&d[p]<0) return false;
    return g.status==(!g.undoScore?2:g.level==2&&g.board[g.cursor]=='>'?1:0);
}
} }
