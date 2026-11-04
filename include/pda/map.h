#pragma once
#include <stddef.h>
#include <stdint.h>
#include <vector>
#include <string>
#include "pda/flash_records.h"

namespace pda { namespace maps {
struct Point { int16_t x, y; };
struct Coord { int32_t x, y; };
// Kinds: river=0, local street=1, avenue=2, arterial=3, cycleway=4, railway=5.
// These immutable flash records need no alignment padding. Keep all map data
// while leaving room for the BLE stack in the existing application partition.
#pragma pack(push, 1)
struct Feature {
    FlashOffset24 first;
    uint8_t count;
    uint8_t kind;
    FlashOffset24 name;
    int16_t minX, minY, maxX, maxY;
};
#pragma pack(pop)
static_assert(sizeof(Feature) == 16, "Compact map feature flash layout");
struct Bounds { int32_t minX, minY, maxX, maxY; };
extern const Point points[];
extern const Feature features[];
// Name IDs refer to bounded, lossless flash string blocks.
std::string featureName(uint32_t name);
extern const size_t pointCount, featureCount;
extern const Bounds coverage;
struct SpatialCell { uint32_t first,count; };
extern const int gridMeters,gridColumns,gridRows;
extern const SpatialCell spatialCells[];
extern const uint16_t spatialIds[];
extern const size_t spatialIdCount;
void queryFeatures(const Bounds &bounds,std::vector<uint32_t> &indices);
struct SearchResult { uint32_t feature; Coord point; };
// Street names only: no house-number geocoding or network requests.
void searchStreets(const std::string &query,Coord near,std::vector<SearchResult> &results,size_t limit=5);
std::string normalizeSearch(const std::string &text);
struct AddressResult {
    uint32_t street,postcode;
    Coord point;
    uint16_t number,from,to;
};
// Returns true for a comma-separated address query, including invalid/unknown
// numbers. Empty results never fall back to a fabricated house position.
bool searchApproximateAddresses(const std::string &query,Coord near,std::vector<AddressResult> &results,size_t limit=3);
std::string addressStreetName(uint32_t street);
bool validateAddressData();
struct RenderStats {
    uint32_t geometryBuilds=0, lastBuildMs=0, candidates=0, strokes=0;
};
const RenderStats &renderStats();
constexpr double originLat = -23.64, originLon = -46.70;
bool project(double latitude, double longitude, Coord &point);
void unproject(Coord point, double &latitude, double &longitude);
bool contains(Coord point);
struct Position {
    bool valid = false, inside = false;
    Coord point = {0,0};
    unsigned satellites = 0;
    uint32_t revision = 0;
};
void updateGps(bool valid, double latitude, double longitude, unsigned satellites);
const Position &gps();
class View {
public:
    Coord center = {0,0};
    uint8_t zoom = 4;
    bool follow = false;
    View();
    void reset();
    void restore(int32_t x, int32_t y, int level);
    void pan(int dx, int dy, int pixels = 16);
    void changeZoom(int direction);
    bool followGps();
    int metersPerPixel() const;
    void screen(Coord point, int width, int height, int &x, int &y) const;
    bool segment(Point a, Point b, int width, int height, int &x0, int &y0, int &x1, int &y1) const;
    bool visible(const Feature &feature, int width, int height) const;
private:
    void clamp();
};
} }
