#include "pda/more_games.h"
#include "pda/model.h"
#include "pda/storage.h"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
using namespace pda;
using namespace pda::games;
namespace {
void navalTests() {
    Save g; reset(Kind::Naval,g,27); assert(valid(Kind::Naval,g));
    g.cursor=7; assert(!activate(Kind::Naval,g)); assert(extraSpace(Kind::Naval,g)); assert(activate(Kind::Naval,g));
    assert(g.level==1&&valid(Kind::Naval,g));
    assert(extraSpace(Kind::Naval,g)); g.cursor=0; assert(activate(Kind::Naval,g));
    g.cursor=32; assert(activate(Kind::Naval,g)); g.cursor=48; assert(activate(Kind::Naval,g));
    assert(g.level==4&&valid(Kind::Naval,g));
    g.cursor=0; assert(activate(Kind::Naval,g)); const auto previous=g;
    assert(!activate(Kind::Naval,g)&&g.marks==previous.marks&&g.seed==previous.seed&&g.moves==previous.moves);
    reset(Kind::Naval,g,55); assert(special(Kind::Naval,g)&&valid(Kind::Naval,g));
    unsigned last=0;
    for(unsigned p=64;p<128;++p) if(g.board[p]!='.') { if(g.score<11) { g.marks[p]='x'; ++g.score; ++g.moves; } else last=p-64; }
    g.cursor=uint8_t(last); assert(valid(Kind::Naval,g)); assert(activate(Kind::Naval,g));
    assert(g.status==1&&g.score==12&&valid(Kind::Naval,g));
    reset(Kind::Naval,g,7); special(Kind::Naval,g);
    unsigned unhit=0;
    for(unsigned p=0;p<64;++p) {
        if(g.board[p]=='.') g.marks[p]='o';
        else if(g.undoScore<11) { g.marks[p]='x'; ++g.undoScore; } else unhit=p;
    }
    assert(g.marks[unhit]=='?'); g.cursor=uint8_t(g.board.find('.',64)-64);
    assert(activate(Kind::Naval,g)&&g.status==2&&valid(Kind::Naval,g));
}
void snakeTests() {
    Save g; reset(Kind::Snake,g,9); assert(g.undoStatus&&valid(Kind::Snake,g));
    assert(!snakeStep(g)); g.marks="44";
    assert(activate(Kind::Snake,g)&&snakeStep(g)&&g.score==1&&snakeBody(g).size()==4&&valid(Kind::Snake,g));
    assert(!move(Kind::Snake,g,-1,0));
    assert(move(Kind::Snake,g,0,-1)); assert(!move(Kind::Snake,g,-1,0));
    assert(snakeStep(g)&&valid(Kind::Snake,g)); assert(snakePause(g));
    const auto paused=g; assert(!snakeStep(g)&&g.board==paused.board);
    reset(Kind::Snake,g,4); activate(Kind::Snake,g);
    for(unsigned i=0;i<20&&!g.status;++i) { snakeStep(g); assert(valid(Kind::Snake,g)); }
    assert(g.status==2&&g.undoStatus);
    reset(Kind::Snake,g,1); g.board="333444434232"; g.marks="00"; g.score=3; g.cursor=2; g.undoCursor=1; g.undoStatus=0;
    assert(valid(Kind::Snake,g)); assert(snakeStep(g)&&g.status==2&&valid(Kind::Snake,g));
}
void codeTests() {
    Save g; reset(Kind::Code,g,12); assert(valid(Kind::Code,g));
    g.board="1122"; g.undoBoard="1212"; assert(activate(Kind::Code,g)&&g.marks=="121222"&&valid(Kind::Code,g));
    reset(Kind::Code,g,12); g.board="1112"; g.undoBoard="2222"; activate(Kind::Code,g);
    assert(g.marks=="222210"&&valid(Kind::Code,g));
    reset(Kind::Code,g,12); g.board="1234";
    for(unsigned n=1;n<=4;++n) assert(codeDigit(g,n));
    assert(g.cursor==0&&activate(Kind::Code,g)&&g.status==1&&valid(Kind::Code,g));
    reset(Kind::Code,g,12); g.board="6666";
    for(unsigned i=0;i<10;++i) assert(activate(Kind::Code,g)&&valid(Kind::Code,g));
    assert(g.status==2&&!activate(Kind::Code,g));
}
std::vector<unsigned> locate(const Save &g,const std::string &raw) {
    auto word=lowerAscii(raw); for(char &c:word) c=char(c-'a'+'A');
    const int width=int(searchWidth(g)),height=int(searchHeight(g));
    for(unsigned p=0;p<g.board.size();++p) for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        if(!dx&&!dy) continue;
        const int x=int(p%width)+dx*int(word.size()-1),y=int(p/width)+dy*int(word.size()-1);
        if(x<0||x>=width||y<0||y>=height) continue;
        const auto path=searchPath(g,p,unsigned(y*width+x)); std::string candidate;
        for(unsigned q:path) candidate+=g.board[q];
        if(candidate==word) return path;
    }
    return {};
}
Save legacySearch(bool compact) {
    Save g; g.started=true; g.board.assign(80,'X'); g.undoBoard.assign(80,'0');
    unsigned row=0;
    for(unsigned id:{0u,1u,2u,4u,5u,6u}) {
        auto word=lowerAscii(wordAt(id)); for(char &c:word) c=char(c-'a'+'A');
        g.board.replace(row++*10,word.size(),word);
        if(!compact) g.marks+="00";
        g.marks+="00"; g.marks.back()="0123456789abcdef"[id];
    }
    return g;
}
void solveSearch(Save &g) {
    for(unsigned i=0;i<searchCount(g);++i) {
        const auto path=locate(g,searchWord(g,i)); assert(!path.empty());
        g.cursor=uint8_t(i%2?path.back():path.front()); assert(activate(Kind::WordSearch,g));
        g.cursor=uint8_t(i%2?path.front():path.back()); assert(activate(Kind::WordSearch,g));
        assert((g.score&(1u<<i))&&valid(Kind::WordSearch,g));
    }
    assert(g.status==1&&g.best==1);
}
void wordTests() {
    for(unsigned seed=1;seed<=20;++seed) for(unsigned theme=0;theme<extraLevels(Kind::WordSearch);++theme) {
        Save g; reset(Kind::WordSearch,g,seed,theme); assert(valid(Kind::WordSearch,g));
        assert(searchPath(g,0,18).empty()&&searchPath(g,15,16).empty());
        assert(g.board.size()==208&&g.marks.size()==40&&searchCount(g)==10);
        if(seed==1&&theme==23) {
            State state; state.games.slots[8]=g; std::vector<uint8_t> bytes;
            assert(encodeSnapshot(state,2,bytes)); State restored; uint64_t generation;
            assert(decodeSnapshot(bytes,restored,generation)&&restored.games.slots[8].marks==g.marks&&restored.games.slots[8].board==g.board);
            auto bad=g; bad.marks.replace(0,4,"ffff"); assert(!valid(Kind::WordSearch,bad));
            auto edge=g; edge.cursor=207;
            assert(!move(Kind::WordSearch,edge,1,0)&&!move(Kind::WordSearch,edge,0,1));
        }
        solveSearch(g);
    }
    for(bool compact:{false,true}) {
        auto g=legacySearch(compact); assert(valid(Kind::WordSearch,g));
        State state; state.games.slots[8]=g; std::vector<uint8_t> bytes;
        assert(encodeSnapshot(state,1,bytes)); State restored; uint64_t generation;
        assert(decodeSnapshot(bytes,restored,generation)&&restored.games.slots[8].marks==g.marks);
        g=restored.games.slots[8]; solveSearch(g);
    }
}
std::vector<unsigned> route(const Save &g) {
    const int width=int(dungeonWidth(g)); const int size=int(g.board.size());
    std::vector<int> previous(size,-1); std::vector<unsigned> queue={g.cursor}; previous[g.cursor]=g.cursor;
    const unsigned goal=unsigned(g.board.find('>'));
    for(size_t i=0;i<queue.size()&&previous[goal]<0;++i) for(int delta:{-width,-1,1,width}) {
        const int p=int(queue[i])+delta;
        if(p>=0&&p<size&&std::abs(p%width-int(queue[i]%width))+std::abs(p/width-int(queue[i]/width))==1&&g.board[unsigned(p)]!='#'&&previous[unsigned(p)]<0) { previous[unsigned(p)]=int(queue[i]); queue.push_back(unsigned(p)); }
    }
    assert(previous[goal]>=0); std::vector<unsigned> path;
    for(unsigned p=goal;p!=g.cursor;p=unsigned(previous[p])) path.push_back(p);
    std::reverse(path.begin(),path.end()); return path;
}
void dungeonTests() {
    for(unsigned seed=1;seed<=50;++seed) {
        Save g; reset(Kind::Dungeon,g,seed); assert(valid(Kind::Dungeon,g));
        for(unsigned floor=0;floor<3;++floor) {
            for(char &c:g.board) if(dungeonEnemy(c)) c='.';
            const auto path=route(g);
            for(unsigned p:path) { const int dx=int(p%dungeonWidth(g))-int(g.cursor%dungeonWidth(g)),dy=int(p/dungeonWidth(g))-int(g.cursor/dungeonWidth(g)); assert(move(Kind::Dungeon,g,dx,dy)&&valid(Kind::Dungeon,g)); }
        }
        assert(g.status==1&&g.score>=75);
    }
    Save g; reset(Kind::Dungeon,g,11);
    for(char &c:g.board) if(dungeonEnemy(c)) c='.';
    g.undoScore=4; assert(special(Kind::Dungeon,g)&&g.undoScore==10&&g.undoMoves==1&&valid(Kind::Dungeon,g));
    reset(Kind::Dungeon,g,1); unsigned enemy=g.board[21]=='#'?39:21;
    g.board[enemy]='O'; g.undoScore=1; g.moves=1;
    assert(move(Kind::Dungeon,g,int(enemy%19)-1,int(enemy/19)-1)&&g.status==2&&valid(Kind::Dungeon,g));
}
Save arena() {
    Save g; reset(Kind::Dungeon,g,7);
    g.board.assign(247,'#'); g.marks.assign(247,'1');
    for(unsigned y=1;y<12;++y) for(unsigned x=1;x<18;++x) g.board[y*19+x]='.';
    g.cursor=100; g.board[226]='>'; return g;
}
void dungeonCombatTests() {
    auto g=arena(); g.board[104]='E';
    assert(activate(Kind::Dungeon,g)&&g.board[103]=='E'&&g.board[104]=='.');
    g=arena(); g.board[104]='O';
    assert(activate(Kind::Dungeon,g)&&g.board[104]=='O');
    assert(activate(Kind::Dungeon,g)&&g.board[103]=='O');
    g=arena(); g.board[101]='O';
    assert(move(Kind::Dungeon,g,1,0)&&g.board[101]=='o'&&g.undoScore==20);
    assert(move(Kind::Dungeon,g,1,0)&&g.board[101]=='.'&&g.score==6&&valid(Kind::Dungeon,g));
    g=arena(); g.board[101]='E'; const auto before=g;
    assert(dungeonAction(g,'f')&&g.undoScore==20&&g.moves==before.moves+1);
    assert(activate(Kind::Dungeon,g)&&g.undoScore==19);
    g=arena(); g.board[101]='E';
    assert(dungeonAction(g,'Q')&&g.undoSeed==5&&dungeonSupply(g,1)==0&&g.undoScore==20);
    const auto turn=g.moves; assert(!dungeonAction(g,'q')&&g.moves==turn);
    g.undoBoard[1]='1'; assert(!dungeonAction(g,'q')&&g.moves==turn&&g.undoBoard[1]=='1');
    for(unsigned i=0;i<5;++i) assert(activate(Kind::Dungeon,g)&&g.undoScore==20);
    assert(g.undoSeed==0&&activate(Kind::Dungeon,g)&&g.undoScore==19);
    g=arena(); g.undoScore=18;
    assert(special(Kind::Dungeon,g)&&g.undoScore==20&&g.undoMoves==1);
    const auto full=g; assert(!special(Kind::Dungeon,g)&&g.moves==full.moves&&g.undoMoves==full.undoMoves);
    g=arena(); g.board[101]='O'; g.board[102]='B';
    assert(dungeonAction(g,'x')&&g.score==9&&dungeonSupply(g,0)==0&&g.undoScore==20&&valid(Kind::Dungeon,g));
    const auto spent=g.moves; assert(!dungeonAction(g,'x')&&g.moves==spent);
    g=arena(); g.board[101]='#'; g.board[102]='O';
    assert(dungeonAction(g,'x')&&g.score==0); // blast follows paths, never crosses walls
    g=arena(); g.board[101]='Q'; g.board[102]='X'; g.board[103]='P';
    assert(move(Kind::Dungeon,g,1,0)&&dungeonSupply(g,1)==2);
    assert(move(Kind::Dungeon,g,1,0)&&dungeonSupply(g,0)==2);
    assert(move(Kind::Dungeon,g,1,0)&&g.undoMoves==3&&valid(Kind::Dungeon,g));
    auto bad=g; bad.undoBoard="a0"; assert(!valid(Kind::Dungeon,bad));
    bad=g; bad.undoSeed=7; assert(!valid(Kind::Dungeon,bad));
    // Original 11x9 adventure stays playable, including its exit and fog.
    g=arena(); g.board.assign(99,'#'); g.marks.assign(99,'0'); g.undoBoard.clear(); g.cursor=12; g.undoScore=12;
    for(unsigned y=1;y<8;++y) for(unsigned x=1;x<10;++x) g.board[y*11+x]='.';
    g.board[86]='>'; g.marks[12]='1';
    assert(valid(Kind::Dungeon,g)&&move(Kind::Dungeon,g,1,0)&&g.board.size()==99);
    State state; g.undoSeed=1; state.games.slots[9]=g;
    std::vector<uint8_t> bytes; assert(encodeSnapshot(state,8,bytes)); State restored; uint64_t generation;
    assert(decodeSnapshot(bytes,restored,generation)&&restored.games.slots[9].board==g.board&&restored.games.slots[9].undoSeed==0);
    g=restored.games.slots[9]; const auto legacyPath=route(g);
    for(unsigned p:legacyPath) assert(move(Kind::Dungeon,g,int(p%11)-int(g.cursor%11),int(p/11)-int(g.cursor/11)));
    assert(g.level==1&&g.board.size()==247&&valid(Kind::Dungeon,g));
    g=arena(); g.undoScore=13; assert(dungeonAction(g,'q')); state.games.slots[9]=g;
    assert(encodeSnapshot(state,9,bytes)&&decodeSnapshot(bytes,restored,generation));
    const auto &saved=restored.games.slots[9];
    assert(saved.undoSeed==5&&saved.undoBoard==g.undoBoard&&saved.undoScore==13&&saved.moves==g.moves&&saved.cursor==g.cursor);
}
void fuzzTests() {
    uint32_t r=5;
    for(unsigned k=5;k<10;++k) {
        Save g; reset(Kind(k),g,19);
        for(unsigned i=0;i<800;++i) {
            r=r*1664525u+1013904223u; const unsigned op=r%11;
            if(op<4) move(Kind(k),g,op==0?-1:op==1?1:0,op==2?-1:op==3?1:0);
            else if(op==4) activate(Kind(k),g);
            else if(op==5) special(Kind(k),g);
            else if(op==6) extraSpace(Kind(k),g);
            else if(k==9) dungeonAction(g,op==8?'q':op==9?'x':'f');
            else if(k==7) codeDigit(g,1+r%6);
            if(k==6) snakeStep(g);
            assert(valid(Kind(k),g));
            if(g.status&&i%7==0) reset(Kind(k),g,r,i%8);
        }
    }
}
}
void moreGameTests() {
    navalTests(); snakeTests(); codeTests(); wordTests(); dungeonTests(); dungeonCombatTests(); fuzzTests();
    puts("more games: fleets/AI/repeats/win/loss, snake timing/collisions, duplicate-code feedback, 480 solvable word grids, 150 connected dungeon floors, combat/shields/bombs/legacy saves and action fuzz OK");
}
