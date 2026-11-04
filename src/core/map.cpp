#include "pda/map.h"
#include "pda/flash_strings.h"
#include <algorithm>
#include <cmath>

namespace pda { namespace maps {
#include "map_data.inc"
std::string featureName(uint32_t name) { return mapNames.get(name); }
namespace {
constexpr double pi = 3.14159265358979323846;
const double mx = 111319.49079327358 * std::cos(originLat*pi/180.0);
constexpr double my = 111319.49079327358;
const int scales[] = {1,2,4,8,16,32,64,128,256};
Position position;
}
bool project(double lat, double lon, Coord &p) {
    if (!std::isfinite(lat) || !std::isfinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180) return false;
    p.x = int32_t(std::lround((lon-originLon)*mx));
    p.y = int32_t(std::lround((originLat-lat)*my));
    return true;
}
void unproject(Coord p, double &lat, double &lon) { lat = originLat-p.y/my; lon = originLon+p.x/mx; }
bool contains(Coord p) { return p.x>=coverage.minX && p.x<=coverage.maxX && p.y>=coverage.minY && p.y<=coverage.maxY; }
void queryFeatures(const Bounds &b,std::vector<uint32_t> &out) {
    out.clear();
    if(b.minX>b.maxX || b.minY>b.maxY || b.maxX<coverage.minX || b.minX>coverage.maxX ||
        b.maxY<coverage.minY || b.minY>coverage.maxY) return;
    const int x0=(std::max(b.minX,coverage.minX)-coverage.minX)/gridMeters;
    const int x1=(std::min(b.maxX,coverage.maxX)-coverage.minX)/gridMeters;
    const int y0=(std::max(b.minY,coverage.minY)-coverage.minY)/gridMeters;
    const int y1=(std::min(b.maxY,coverage.maxY)-coverage.minY)/gridMeters;
    for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
        const auto &cell=spatialCells[y*gridColumns+x];
        out.insert(out.end(),spatialIds+cell.first,spatialIds+cell.first+cell.count);
    }
    std::sort(out.begin(),out.end()); out.erase(std::unique(out.begin(),out.end()),out.end());
    out.erase(std::remove_if(out.begin(),out.end(),[&b](uint32_t i) {
        const auto &f=features[i];
        return f.maxX<b.minX || f.minX>b.maxX || f.maxY<b.minY || f.minY>b.maxY;
    }),out.end());
}
std::string normalizeSearch(const std::string &text) {
    std::string out;
    for(size_t i=0;i<text.size();++i) {
        unsigned char c=static_cast<unsigned char>(text[i]);
        if(c==0xC3 && i+1<text.size()) {
            const unsigned char next=static_cast<unsigned char>(text[++i]);
            if((next>=0x80&&next<=0x85)||(next>=0xA0&&next<=0xA5)) c='a';
            else if(next==0x87||next==0xA7) c='c';
            else if((next>=0x88&&next<=0x8B)||(next>=0xA8&&next<=0xAB)) c='e';
            else if((next>=0x8C&&next<=0x8F)||(next>=0xAC&&next<=0xAF)) c='i';
            else if((next>=0x92&&next<=0x96)||(next>=0xB2&&next<=0xB6)) c='o';
            else if((next>=0x99&&next<=0x9C)||(next>=0xB9&&next<=0xBC)) c='u';
            else c=' ';
        }
        if(c>='A'&&c<='Z') c=uint8_t(c-'A'+'a');
        if((c>='a'&&c<='z')||(c>='0'&&c<='9')) out+=char(c);
        else if(!out.empty()&&out.back()!=' ') out+=' ';
    }
    if(!out.empty()&&out.back()==' ') out.pop_back();
    if(out.compare(0,8,"avenida ")==0) out.replace(0,8,"av ");
    if(out.compare(0,4,"rua ")==0) out.replace(0,4,"r ");
    if(out.compare(0,6,"ponte ")==0) out.replace(0,6,"pte ");
    if(out.compare(0,8,"estrada ")==0) out.replace(0,8,"est ");
    // Address sources often differ only in de/do/da. Compare whole words so
    // names such as Doria are preserved and alternatives remain selectable.
    std::string filtered;
    for(size_t first=0;first<out.size();) {
        const auto end=out.find(' ',first);
        const auto word=out.substr(first,end==std::string::npos?out.size()-first:end-first);
        if(word!="de" && word!="do" && word!="da" && word!="dos" && word!="das") {
            if(!filtered.empty()) filtered+=' ';
            filtered+=word;
        }
        if(end==std::string::npos) break;
        first=end+1;
    }
    return filtered;
}
void searchStreets(const std::string &query,Coord near,std::vector<SearchResult> &results,size_t limit) {
    results.clear(); limit=std::min<size_t>(8,limit);
    const auto needle=normalizeSearch(query);
    if(needle.size()<2 || !limit) return;
    // Match each unique street name once, then rank its geometric segments.
    // This avoids repeatedly decoding/normalizing the same name on long roads.
    FlashStrings::Cursor names(mapNames);
    std::vector<uint32_t> matching;
    for(size_t id=1;id<mapNames.count;++id) {
        const auto name=normalizeSearch(names.get(id)); const auto pos=name.find(needle);
        if(pos!=std::string::npos) matching.push_back(uint32_t(id)*2+((pos==0||name[pos-1]==' ')?0:1));
    }
    if(matching.empty()) return;
    struct Match { SearchResult result; int rank; int64_t distance; };
    std::vector<Match> best;
    const auto less=[](const Match &a,const Match &b) {
        return a.rank!=b.rank?a.rank<b.rank:a.distance<b.distance;
    };
    for(size_t i=0;i<featureCount;++i) {
        const auto &f=features[i];
        if(!f.name || f.kind==0 || f.kind==5) continue;
        const auto match=std::lower_bound(matching.begin(),matching.end(),uint32_t(f.name)*2);
        if(match==matching.end() || *match/2!=uint32_t(f.name)) continue;
        const auto p=points[f.first+f.count/2];
        const int64_t dx=int64_t(p.x)-near.x,dy=int64_t(p.y)-near.y;
        const Match candidate={{uint32_t(i),{p.x,p.y}},int(*match&1),dx*dx+dy*dy};
        auto same=std::find_if(best.begin(),best.end(),[&f](const Match &m) { return features[m.result.feature].name==f.name; });
        if(same!=best.end()) { if(less(candidate,*same)) *same=candidate; }
        else best.push_back(candidate);
        std::sort(best.begin(),best.end(),less);
        if(best.size()>limit) best.pop_back();
    }
    for(const auto &m:best) results.push_back(m.result);
}
void updateGps(bool valid, double lat, double lon, unsigned satellites) {
    Coord p = {0,0}; valid = valid && project(lat,lon,p);
    const bool inside = valid && contains(p);
    if (position.valid != valid || position.inside != inside || position.satellites != satellites ||
        (valid && (position.point.x != p.x || position.point.y != p.y))) ++position.revision;
    position.valid=valid; position.inside=inside; position.point=p; position.satellites=satellites;
}
const Position &gps() { return position; }
View::View() { reset(); }
void View::reset() { project(-23.6305,-46.7200,center); zoom=4; follow=false; clamp(); }
void View::clamp() {
    center.x=std::max(coverage.minX,std::min(coverage.maxX,center.x));
    center.y=std::max(coverage.minY,std::min(coverage.maxY,center.y));
}
void View::restore(int32_t x,int32_t y,int level) {
    center={x,y}; zoom=uint8_t(std::max(0,std::min(8,level))); follow=false; clamp();
}
int View::metersPerPixel() const { return scales[std::min<unsigned>(zoom,8)]; }
void View::pan(int dx,int dy,int pixels) {
    pixels=std::max(1,std::min(16,pixels));
    center.x += std::max(-8,std::min(8,dx))*pixels*metersPerPixel();
    center.y += std::max(-8,std::min(8,dy))*pixels*metersPerPixel();
    follow=false; clamp();
}
void View::changeZoom(int direction) { zoom=uint8_t(std::max(0,std::min(8,int(zoom)+(direction>0?-1:1)))); }
bool View::followGps() {
    if (!position.valid || !position.inside) return false;
    center=position.point; follow=true; return true;
}
void View::screen(Coord p,int w,int h,int &x,int &y) const {
    x=w/2+(p.x-center.x)/metersPerPixel(); y=h/2+(p.y-center.y)/metersPerPixel();
}
bool View::visible(const Feature &f,int w,int h) const {
    const int hx=(w/2+1)*metersPerPixel(), hy=(h/2+1)*metersPerPixel();
    return f.maxX>=center.x-hx && f.minX<=center.x+hx && f.maxY>=center.y-hy && f.minY<=center.y+hy;
}
bool View::segment(Point a,Point b,int w,int h,int &x0,int &y0,int &x1,int &y1) const {
    if(w<=0||h<=0) return false;
    screen({a.x,a.y},w,h,x0,y0); screen({b.x,b.y},w,h,x1,y1);
    const auto code=[w,h](int x,int y) { return (x<0?1:x>=w?2:0)|(y<0?4:y>=h?8:0); };
    int c0=code(x0,y0),c1=code(x1,y1);
    // Integer Cohen-Sutherland. Most streets are wholly inside/outside; those
    // cases need no floating-point work or line-intersection division.
    for(int i=0;i<12;++i) {
        if(!(c0|c1)) return true;
        if(c0&c1) return false;
        const int outside=c0?c0:c1;
        int x=0,y=0;
        if(outside&12) {
            if(y1==y0) return false;
            y=(outside&4)?0:h-1;
            x=x0+int(int64_t(x1-x0)*(y-y0)/(y1-y0));
        } else {
            if(x1==x0) return false;
            x=(outside&1)?0:w-1;
            y=y0+int(int64_t(y1-y0)*(x-x0)/(x1-x0));
        }
        if(outside==c0) { x0=x; y0=y; c0=code(x0,y0); }
        else { x1=x; y1=y; c1=code(x1,y1); }
    }
    return false;
}
} }
