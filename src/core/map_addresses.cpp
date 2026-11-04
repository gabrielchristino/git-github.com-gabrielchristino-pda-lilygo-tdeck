#include "pda/map.h"
#include "pda/flash_strings.h"
#include <algorithm>
#include <cstring>

namespace pda { namespace maps {
namespace {
#pragma pack(push, 1)
struct AddressStreet { FlashOffset24 name,first; uint16_t count; };
struct AddressRange { FlashOffset24 first; uint8_t count; uint16_t postcode; };
#pragma pack(pop)
struct AddressAnchor { uint16_t number; int16_t x,y; };
static_assert(sizeof(AddressStreet)==8 && sizeof(AddressRange)==6,"Address flash layout");
#include "map_addresses.inc"
constexpr size_t streetCount=sizeof(addressStreets)/sizeof(addressStreets[0]);
constexpr size_t rangeCount=sizeof(addressRanges)/sizeof(addressRanges[0]);
constexpr size_t anchorBytes=sizeof(addressAnchors);
constexpr size_t postcodeCount=sizeof(addressPostcodes)/sizeof(addressPostcodes[0]);
// Each range starts from zero and stores positive number deltas and zigzag
// coordinate deltas. Decode at most 52 anchors for the current extract, using
// constant RAM; all original coordinates and interpolation rules are retained.
class AnchorReader {
public:
    explicit AnchorReader(uint32_t offset) : position(offset) {}
    uint32_t position;
    AddressAnchor point = {};
    bool next() {
        uint32_t number=0,x=0,y=0;
        if (!read(number) || !read(x) || !read(y) || !number || number>65535u-point.number) return false;
        const int32_t nx=int32_t(point.x)+signedDelta(x), ny=int32_t(point.y)+signedDelta(y);
        if(nx < -32768 || nx > 32767 || ny < -32768 || ny > 32767) return false;
        point={uint16_t(point.number+number),int16_t(nx),int16_t(ny)};
        return true;
    }
private:
    static int32_t signedDelta(uint32_t v) { return (v&1) ? -int32_t((v>>1)+1) : int32_t(v>>1); }
    bool read(uint32_t &value) {
        value=0;
        for(unsigned shift=0;shift<=14;shift+=7) {
            if(position>=anchorBytes) return false;
            const uint8_t byte=addressAnchors[position++];
            value|=uint32_t(byte&127)<<shift;
            if(!(byte&128)) return value<=131070;
        }
        return false;
    }
};
bool estimate(const AddressRange &range,uint16_t number,Coord &point) {
    AnchorReader reader(range.first);
    if(!range.count || !reader.next() || number<reader.point.number || number%2!=reader.point.number%2) return false;
    for(unsigned i=0;i<range.count;++i) {
        const auto b=reader.point;
        if(b.number==number) { point={b.x,b.y}; return true; }
        if(i+1==range.count) return false;
        if(!reader.next()) return false;
        const auto next=reader.point;
        if(number<next.number) {
            point={b.x+int32_t(int64_t(next.x-b.x)*(number-b.number)/(next.number-b.number)),
                   b.y+int32_t(int64_t(next.y-b.y)*(number-b.number)/(next.number-b.number))};
            return true;
        }
    }
    return false;
}
}
std::string addressStreetName(uint32_t street) {
    return street<streetCount?addressNames.get(addressStreets[street].name):"";
}
bool searchApproximateAddresses(const std::string &query,Coord near,std::vector<AddressResult> &results,size_t limit) {
    results.clear(); limit=std::min<size_t>(5,limit);
    const auto comma=query.rfind(',');
    if(comma==std::string::npos) return false;
    const auto needle=normalizeSearch(query.substr(0,comma));
    size_t pos=comma+1;
    while(pos<query.size() && query[pos]==' ') ++pos;
    uint32_t number=0; bool digit=false;
    while(pos<query.size() && query[pos]>='0' && query[pos]<='9') {
        digit=true; number=number*10+uint32_t(query[pos++]-'0');
        if(number>65535) return true;
    }
    while(pos<query.size() && query[pos]==' ') ++pos;
    if(pos!=query.size() || !digit || !number || needle.size()<2 || !limit) return true;
    struct Match { AddressResult result; int rank; int64_t distance; };
    std::vector<Match> matches;
    const auto less=[](const Match &a,const Match &b) { return a.rank!=b.rank?a.rank<b.rank:a.distance<b.distance; };
    FlashStrings::Cursor names(addressNames);
    for(size_t sid=0;sid<streetCount;++sid) {
        const auto name=normalizeSearch(names.get(addressStreets[sid].name));
        const auto at=name.find(needle);
        if(at==std::string::npos) continue;
        const int rank=name==needle?0:(at==0 || (at<=4 && name[at-1]==' '))?1:2;
        const auto &street=addressStreets[sid];
        for(uint32_t i=street.first;i<street.first+street.count;++i) {
            const auto &range=addressRanges[i]; Coord point;
            if(!estimate(range,uint16_t(number),point)) continue;
            AnchorReader reader(range.first);
            if(!reader.next()) continue;
            const uint16_t from=reader.point.number;
            bool valid=true;
            for(unsigned j=1;j<range.count;++j) if(!reader.next()) { valid=false; break; }
            if(!valid) continue;
            const int64_t dx=int64_t(point.x)-near.x,dy=int64_t(point.y)-near.y;
            const AddressResult result={uint32_t(sid),addressPostcodes[range.postcode],point,uint16_t(number),from,reader.point.number};
            matches.push_back({result,rank,dx*dx+dy*dy});
            std::sort(matches.begin(),matches.end(),less);
            if(matches.size()>limit) matches.pop_back();
        }
    }
    for(const auto &match:matches) results.push_back(match.result);
    return true;
}
bool validateAddressData() {
    uint32_t nextRange=0,nextByte=0;
    for(size_t sid=0;sid<streetCount;++sid) {
        const auto &s=addressStreets[sid];
        if(s.name>=addressNames.count || addressNames.get(s.name).empty() ||
            s.first+s.count>rangeCount || (s.count && s.first!=nextRange)) return false;
        for(uint32_t i=s.first;i<s.first+s.count;++i) {
            const auto &r=addressRanges[i];
            if(!r.count || r.first!=nextByte || r.first>=anchorBytes || r.postcode>=postcodeCount) return false;
            AnchorReader reader(r.first);
            AddressAnchor first={},previous={};
            for(unsigned j=0;j<r.count;++j) {
                if(!reader.next()) return false;
                const auto a=reader.point;
                if(!j) first=a;
                if(!contains({a.x,a.y}) || a.number%2!=first.number%2 ||
                    (j && a.number<=previous.number)) return false;
                Coord point;
                if(!estimate(r,a.number,point) || point.x!=a.x || point.y!=a.y) return false;
                if(j && a.number-previous.number>=4) {
                    const uint16_t number=uint16_t(previous.number+2);
                    if(!estimate(r,number,point) || point.x<std::min(previous.x,a.x) || point.x>std::max(previous.x,a.x) ||
                        point.y<std::min(previous.y,a.y) || point.y>std::max(previous.y,a.y)) return false;
                }
                if(a.number<65535 && estimate(r,uint16_t(a.number+1),point)) return false;
                previous=a;
            }
            Coord ignored;
            if(first.number>2 && estimate(r,uint16_t(first.number-2),ignored)) return false;
            if(previous.number<=65533 && estimate(r,uint16_t(previous.number+2),ignored)) return false;
            nextByte=reader.position;
        }
        nextRange+=s.count;
    }
    return nextRange==rangeCount && nextByte==anchorBytes;
}
} }
