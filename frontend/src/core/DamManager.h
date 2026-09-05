#pragma once

#include <QString>
#include <vector>
#include <memory>
#include <algorithm>

namespace MapCore {

struct DamPoint {
    QString pic;          // e.g. "AP01MH0035"
    QString name;         // e.g. "C.K Reservoir"
    QString state;        // e.g. "Andhra Pradesh"
    QString district;     // e.g. "Prakasam"
    QString river;        // e.g. "Sarpa"
    QString basin;        // e.g. "Pennar"
    double lat = 0.0;     // WGS84 decimal latitude
    double lon = 0.0;     // WGS84 decimal longitude
    float height = 0.0f;  // Height in meters
    float storage = 0.0f; // Gross Storage Capacity in MCM
    float spillwayCap = 0.0f; // Discharge capacity in m3/s
    int year = 0;         // Completion year
    QString purpose;      // Irrigation, Water Supply, etc.
    QString damType;      // Earthen, Gravity, etc.
    QString incharge;     // Authority
};

class DamManager {
public:
    static constexpr double GRID_MIN_LAT = 5.0;
    static constexpr double GRID_MAX_LAT = 38.0;
    static constexpr double GRID_MIN_LON = 68.0;
    static constexpr double GRID_MAX_LON = 98.0;
    static constexpr double CELL_SIZE = 0.5; // ~55 km spatial cells
    static constexpr int GRID_ROWS = static_cast<int>((GRID_MAX_LAT - GRID_MIN_LAT) / CELL_SIZE) + 1; // 67
    static constexpr int GRID_COLS = static_cast<int>((GRID_MAX_LON - GRID_MIN_LON) / CELL_SIZE) + 1; // 61

private:
    std::vector<DamPoint> dams;
    bool isLoaded = false;
    std::vector<uint32_t> spatialGrid[GRID_ROWS][GRID_COLS];

    void buildSpatialIndex();

public:
    DamManager() = default;

    bool loadFromGeoJson(const QString& filePath = "server/dam.geojson");
    bool loadFromBinary(const QString& cachePath = "data/dams_cache.bin");
    bool saveToBinary(const QString& cachePath = "data/dams_cache.bin") const;

    const std::vector<DamPoint>& getDams() const { return dams; }
    size_t getCount() const { return dams.size(); }
    bool hasData() const { return isLoaded && !dams.empty(); }

    // Fast Frustum Culling Query: extracts only visible dams within the given bounding box
    void getDamsInBbox(double minLat, double minLon, double maxLat, double maxLon, std::vector<const DamPoint*>& outDams) const;

    const DamPoint* findNearest(double lat, double lon, double maxDistDeg = 0.15) const;

    static double parseDmsCoordinate(const QString& dms);
};

} // namespace MapCore
