// Host-only benchmark. Compare checksums as well as timings across versions.
#include "pda/map.h"
#include <chrono>
#include <cstdio>
int main() {
    using namespace pda::maps;
    const char *queries[]={"nacoes unidas","hebe camargo","rua","pinheiros","zzzzzzzz"};
    std::vector<SearchResult> results;
    size_t checksum=0;
    const auto start=std::chrono::steady_clock::now();
    for(int repeat=0;repeat<8;++repeat) for(auto query:queries) {
        searchStreets(query,{0,0},results);
        for(const auto &result:results) checksum+=result.feature;
    }
    const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    std::printf("searches=40 checksum=%lu us=%lld\n",static_cast<unsigned long>(checksum),static_cast<long long>(us));
}
