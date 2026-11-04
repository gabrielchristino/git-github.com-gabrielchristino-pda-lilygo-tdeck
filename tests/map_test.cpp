#include "pda/map.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
int main() {
    using namespace pda::maps;
    assert(featureCount>1000 && pointCount>1000);
    bool river=false,marginal=false;
    for(size_t i=0;i<featureCount;++i) {
        const auto &f=features[i];
        assert(f.count>=2 && f.first+f.count<=pointCount && f.kind<=5);
        const auto name=featureName(f.name);
        assert(name.size()<=100);
        if(f.kind==0 && name.find("Pinheiros")!=std::string::npos) river=true;
        if(name.find("Nações Unidas")!=std::string::npos) marginal=true;
        for(unsigned j=0;j<f.count;++j) {
            auto p=points[f.first+j]; assert(contains({p.x,p.y}));
            assert(p.x>=f.minX && p.x<=f.maxX && p.y>=f.minY && p.y<=f.maxY);
        }
    }
    assert(river && marginal);
    for(int i=0;i<gridColumns*gridRows;++i)
        assert(spatialCells[i].first+spatialCells[i].count<=spatialIdCount);
    for(size_t i=0;i<spatialIdCount;++i) assert(spatialIds[i]<featureCount);
    std::vector<uint32_t> actual,expected;
    uint32_t seed=71;
    for(int trial=0;trial<45;++trial) {
        seed=seed*1664525u+1013904223u; const int x=int(seed%40000)-20000;
        seed=seed*1664525u+1013904223u; const int y=int(seed%40000)-20000;
        const Bounds b=trial==0?coverage:Bounds{x,y,x+1+trial*190,y+1+trial*150};
        queryFeatures(b,actual); expected.clear();
        for(size_t i=0;i<featureCount;++i) {
            const auto &f=features[i];
            if(f.maxX>=b.minX && f.minX<=b.maxX && f.maxY>=b.minY && f.minY<=b.maxY) expected.push_back(uint32_t(i));
        }
        assert(actual==expected);
    }
    queryFeatures({5,5,4,4},actual); assert(actual.empty());
    Coord p; assert(project(-23.63,-46.72,p) && contains(p));
    double lat,lon; unproject(p,lat,lon);
    assert(std::abs(lat+23.63)<0.00001 && std::abs(lon+46.72)<0.00001);
    assert(!project(std::numeric_limits<double>::quiet_NaN(),0,p));
    assert(!project(91,0,p));
    View view; const auto initial=view.center;
    assert(normalizeSearch("Rua das Flores")==normalizeSearch("R. de Flores"));
    assert(normalizeSearch("Avenida do Parque")==normalizeSearch("AVENIDA DE PARQUE"));
    assert(normalizeSearch("Rua Doria")=="r doria");
    assert(normalizeSearch("Avenida das Nações Unidas")==normalizeSearch("Av. de Nacoes Unidas"));
    assert(validateAddressData());
    std::vector<AddressResult> approximate;
    assert(!searchApproximateAddresses("hebe camargo",initial,approximate) && approximate.empty());
    assert(searchApproximateAddresses("hebe camargo, 100",initial,approximate) && !approximate.empty() && approximate.size()<=3);
    bool independentlyCheckedPoint=false;
    for(const auto &a:approximate) {
        assert(a.number==100 && a.from<=100 && a.to>=100 && contains(a.point));
        assert(addressStreetName(a.street).find("HEBE CAMARGO")!=std::string::npos);
        if(std::abs(a.point.x+2144)<=1 && a.point.y==-2656) independentlyCheckedPoint=true;
    }
    assert(independentlyCheckedPoint);
    for(const char *invalid:{"hebe camargo, 65536","hebe camargo, 0","hebe camargo, -100","hebe camargo, 100A","hebe camargo,","zzzzzzzz, 100","hebe camargo, 65000"})
        assert(searchApproximateAddresses(invalid,initial,approximate) && approximate.empty());
    assert(searchApproximateAddresses("hebe camargo, 100",initial,approximate,0) && approximate.empty());
    std::vector<SearchResult> found,accented;
    searchStreets("nacoes unidas",initial,found);
    assert(!found.empty() && found.size()<=5);
    searchStreets("NAÇÕES UNIDAS",initial,accented);
    assert(found.size()==accented.size());
    for(size_t i=0;i<found.size();++i) {
        assert(found[i].feature==accented[i].feature && contains(found[i].point));
        for(size_t j=0;j<i;++j) assert(features[found[i].feature].name!=features[found[j].feature].name);
    }
    searchStreets("Avenida Hebe Camargo",initial,found); assert(!found.empty());
    const auto target=found[0].point;
    searchStreets("hebe camargo",target,accented);
    assert(!accented.empty() && accented[0].point.x==target.x && accented[0].point.y==target.y);
    searchStreets("zzzzzzzzzzzz",initial,found); assert(found.empty());
    searchStreets("  ",initial,found); assert(found.empty());
    searchStreets("a",initial,found); assert(found.empty());
    searchStreets("pinheiros",initial,found,0); assert(found.empty());
    view.pan(1,0,4); assert(view.center.x==initial.x+4*view.metersPerPixel()); view.reset();
    view.pan(1,1); assert(view.center.x>initial.x && view.center.y>initial.y);
    for(int i=0;i<20;++i) view.changeZoom(1);
    assert(view.zoom==0 && view.metersPerPixel()==1);
    view.restore(0,0,0);
    int x0,y0,x1,y1;
    assert(view.segment({-1000,0},{1000,0},296,128,x0,y0,x1,y1));
    assert(x0==0 && x1==295 && y0==64 && y1==64);
    assert(!view.segment({-1000,1000},{1000,1000},296,128,x0,y0,x1,y1));
    assert(!view.segment({0,0},{1,1},0,1,x0,y0,x1,y1));
    for(int i=0;i<20;++i) view.changeZoom(-1);
    assert(view.zoom==8 && view.metersPerPixel()==256);
    for(int i=0;i<100;++i) view.pan(8,8);
    assert(view.center.x==coverage.maxX && view.center.y==coverage.maxY);
    updateGps(false,0,0,0); assert(!view.followGps() && !gps().valid);
    updateGps(true,0,0,8); assert(gps().valid && !gps().inside && !view.followGps());
    updateGps(true,-23.63,-46.72,8); assert(view.followGps() && view.follow);
    assert(view.center.x==gps().point.x && view.center.y==gps().point.y);
    const auto revision=gps().revision;
    updateGps(true,-23.63,-46.72,8); assert(gps().revision==revision);
    view.pan(1,0); assert(!view.follow);
    updateGps(false,0,0,0); assert(!gps().valid && !gps().inside && gps().revision>revision);
    std::cout<<"map: real OSM geometry, bounds, projection, clipping, zoom, panning and GPS validity OK\n";
}
