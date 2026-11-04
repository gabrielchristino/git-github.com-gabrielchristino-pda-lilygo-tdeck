// Include the implementation so host tests can inspect immutable flash records
// without exposing the codec or its tables in the application's public API.
#include "../src/core/map_addresses.cpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace pda;
    using namespace pda::maps;
    for(uint32_t value : {0u, 255u, 256u, 65535u, 65536u, 0xffffffu})
        assert(uint32_t(FlashOffset24(value))==value);

    const FlashOffset24 blocks[]={0};
    const uint8_t namesData[]={0,'a',0,1,'b',0};
    const FlashStrings tiny={namesData,blocks,sizeof(namesData),2};
    const auto firstName=tiny.get(0);
    assert(tiny.get(1)=="ab" && firstName=="a" && tiny.get(2).empty());
    FlashStrings::Cursor cursor(tiny);
    assert(cursor.get(0)=="a" && cursor.get(1)=="ab" && cursor.get(0)=="a");
    assert(cursor.get(1)=="ab" && cursor.get(1)=="ab" && cursor.get(10).empty() && cursor.get(1)=="ab");
    const uint8_t invalidPrefix[]={1,'a',0}, noTerminator[]={0,'a'};
    assert((FlashStrings{invalidPrefix,blocks,sizeof(invalidPrefix),1}.get(0).empty()));
    assert((FlashStrings{noTerminator,blocks,sizeof(noTerminator),1}.get(0).empty()));
    auto hashName=[](uint64_t &hash,const std::string &name) {
        for(unsigned char byte:name) { hash^=byte; hash*=1099511628211ULL; }
        hash^=0; hash*=1099511628211ULL;
    };
    uint64_t featureNamesHash=14695981039346656037ULL, addressNamesHash=14695981039346656037ULL;
    for(size_t i=0;i<featureCount;++i) hashName(featureNamesHash,featureName(features[i].name));
    for(size_t i=0;i<streetCount;++i) hashName(addressNamesHash,addressStreetName(uint32_t(i)));
    // Digests of the original uncompressed names in feature/street order.
    assert(featureCount==62283 && featureNamesHash==0xf4a9a4adb02aa536ULL);
    assert(streetCount==20846 && addressNamesHash==0xcda64205ca1919d6ULL);
    FlashStrings::Cursor sequentialNames(addressNames);
    for(size_t i=0;i<addressNames.count;++i)
        assert(sequentialNames.get(i)==addressNames.get(i));

    uint64_t fingerprint=14695981039346656037ULL;
    auto hash16=[&](uint16_t value) {
        for(unsigned shift=0;shift<16;shift+=8) {
            fingerprint^=uint8_t(value>>shift);
            fingerprint*=1099511628211ULL;
        }
    };
    size_t count=0,interpolations=0;
    uint32_t end=0;
    for(const auto &range : addressRanges) {
        assert(uint32_t(range.first)==end);
        AnchorReader reader(range.first);
        AddressAnchor previous={};
        for(unsigned j=0;j<range.count;++j) {
            assert(reader.next());
            const auto current=reader.point;
            hash16(current.number); hash16(uint16_t(current.x)); hash16(uint16_t(current.y));
            ++count;
            if(j) for(uint32_t number=previous.number;number<=current.number;number+=2) {
                const Coord expected={
                    previous.x+int32_t(int64_t(current.x-previous.x)*(number-previous.number)/(current.number-previous.number)),
                    previous.y+int32_t(int64_t(current.y-previous.y)*(number-previous.number)/(current.number-previous.number))
                };
                Coord actual;
                assert(estimate(range,uint16_t(number),actual));
                assert(actual.x==expected.x && actual.y==expected.y);
                ++interpolations;
            }
            previous=current;
        }
        end=reader.position;
    }
    // FNV-1a of the original 223,204 <Hhh records, computed from the unchanged
    // PDAINT02 50 m source (SHA-256 recorded in interpolation-analysis.json).
    assert(count==223204 && fingerprint==0x1b9d2ee75163ed7bULL && end==anchorBytes);
    AnchorReader pastEnd(anchorBytes);
    assert(!pastEnd.next());
    std::printf("map encoding: all original names and %lu anchors preserved; %lu interpolations unchanged; compact offsets and bounds OK\n",
        static_cast<unsigned long>(count),static_cast<unsigned long>(interpolations));
}
