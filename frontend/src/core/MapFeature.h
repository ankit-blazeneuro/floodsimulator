#pragma once

#include "GeoTypes.h"
#include <string>
#include <vector>
#include <utility>

namespace MapCore {

enum class FeatureCategory : uint8_t {
    // Water
    WATER_OCEAN = 0,
    WATER_RIVER,
    WATER_LAKE,
    WATER_STREAM,
    WATER_CANAL,

    // Landuse & Natural
    LANDUSE_FOREST,
    LANDUSE_PARK,
    LANDUSE_RESIDENTIAL,
    LANDUSE_COMMERCIAL,
    LANDUSE_INDUSTRIAL,
    LANDUSE_FARMLAND,
    LANDUSE_NATURE_RESERVE,

    // Roads & Highways
    HIGHWAY_MOTORWAY,    // National Expressways / NH
    HIGHWAY_TRUNK,       // National Highways
    HIGHWAY_PRIMARY,     // State Highways
    HIGHWAY_SECONDARY,   // Major Inter-district
    HIGHWAY_TERTIARY,    // District roads
    HIGHWAY_RESIDENTIAL, // Urban / town streets
    HIGHWAY_UNCLASSIFIED,// Local roads
    HIGHWAY_SERVICE,     // Access roads / parking
    HIGHWAY_TRACK,       // Rural tracks
    HIGHWAY_PATH,        // Paths & Footways

    // Rail & Air
    RAILWAY_MAIN,
    AEROWAY_RUNWAY,
    AEROWAY_TAXIWAY,
    AEROWAY_APRON,

    // Buildings
    BUILDING,

    // Boundaries
    BOUNDARY_STATE,
    BOUNDARY_DISTRICT,

    // Places (Points & Labels)
    PLACE_STATE,
    PLACE_CITY,
    PLACE_TOWN,
    PLACE_SUBURB,
    PLACE_VILLAGE,
    PLACE_HAMLET,
    PLACE_LOCALITY,

    // POIs
    POI_AIRPORT,
    POI_HOSPITAL,
    POI_UNIVERSITY,
    POI_SCHOOL,
    POI_RAILWAY_STATION,
    POI_WORSHIP,
    POI_TOURISM,
    POI_FUEL,
    POI_BANK,
    POI_HOTEL,
    POI_RESTAURANT,

    UNKNOWN
};

// Polyline Feature (Roads, Rivers, Railways, Boundaries)
struct MapPolyline {
    int64_t id = 0;
    FeatureCategory category = FeatureCategory::UNKNOWN;
    std::string name;
    std::string ref; // e.g. "NH 27", "AH 1"
    BoundingBox bbox;
    std::vector<Point2D> points;      // Full detailed geometry
    std::vector<Point2D> lodPoints;   // Simplified for low-mid zoom levels
    bool isBridge = false;
    bool isTunnel = false;
    bool isOneway = false;
    std::vector<std::pair<std::string, std::string>> tags;

    // Minimum and maximum zoom levels this polyline is visible
    float minZoom = 0.0f;
    float maxZoom = 22.0f;
};

// Polygon Feature (Water bodies, Forests, Parks, Landuse, Buildings)
struct MapPolygon {
    int64_t id = 0;
    FeatureCategory category = FeatureCategory::UNKNOWN;
    std::string name;
    BoundingBox bbox;
    std::vector<Point2D> points;
    std::vector<std::pair<std::string, std::string>> tags;

    float minZoom = 0.0f;
    float maxZoom = 22.0f;
};

// Point Feature (Places, POIs, Labels)
struct MapPoint {
    int64_t id = 0;
    FeatureCategory category = FeatureCategory::UNKNOWN;
    Point2D pos;
    std::string name;
    std::string categoryLabel; // e.g. "Major City", "Hospital", "National Highway"
    uint8_t priority = 5;      // 1 (highest, e.g. Guwahati) to 10 (lowest)
    uint32_t population = 0;
    std::vector<std::pair<std::string, std::string>> tags;

    float minZoom = 0.0f;
    float maxZoom = 22.0f;
};

// Search Result / Quick Index item
struct SearchItem {
    std::string name;
    std::string detail;
    FeatureCategory category;
    Point2D pos;
    BoundingBox bounds;
    float zoomTarget = 12.0f;
    int priority = 5;
};

inline const char* getCategoryDisplayName(FeatureCategory cat) {
    switch (cat) {
        case FeatureCategory::WATER_RIVER: return "River";
        case FeatureCategory::WATER_LAKE: return "Lake";
        case FeatureCategory::WATER_STREAM: return "Stream";
        case FeatureCategory::WATER_CANAL: return "Canal";
        case FeatureCategory::LANDUSE_FOREST: return "Forest / Reserve";
        case FeatureCategory::LANDUSE_PARK: return "Park / Green Area";
        case FeatureCategory::LANDUSE_RESIDENTIAL: return "Residential Area";
        case FeatureCategory::LANDUSE_FARMLAND: return "Farmland / Tea Estate";
        case FeatureCategory::HIGHWAY_MOTORWAY: return "Expressway / NH";
        case FeatureCategory::HIGHWAY_TRUNK: return "National Highway";
        case FeatureCategory::HIGHWAY_PRIMARY: return "State Highway";
        case FeatureCategory::HIGHWAY_SECONDARY: return "Secondary Highway";
        case FeatureCategory::HIGHWAY_TERTIARY: return "Tertiary Road";
        case FeatureCategory::HIGHWAY_RESIDENTIAL: return "Residential Road";
        case FeatureCategory::HIGHWAY_UNCLASSIFIED: return "Local Road";
        case FeatureCategory::HIGHWAY_TRACK: return "Track / Trail";
        case FeatureCategory::HIGHWAY_PATH: return "Footpath / Path";
        case FeatureCategory::RAILWAY_MAIN: return "Railway Line";
        case FeatureCategory::AEROWAY_RUNWAY: return "Airport Runway";
        case FeatureCategory::BUILDING: return "Building";
        case FeatureCategory::PLACE_STATE: return "State";
        case FeatureCategory::PLACE_CITY: return "City";
        case FeatureCategory::PLACE_TOWN: return "Town";
        case FeatureCategory::PLACE_SUBURB: return "Suburb";
        case FeatureCategory::PLACE_VILLAGE: return "Village";
        case FeatureCategory::PLACE_HAMLET: return "Hamlet";
        case FeatureCategory::POI_AIRPORT: return "Airport";
        case FeatureCategory::POI_HOSPITAL: return "Hospital / Healthcare";
        case FeatureCategory::POI_UNIVERSITY: return "University";
        case FeatureCategory::POI_SCHOOL: return "School";
        case FeatureCategory::POI_RAILWAY_STATION: return "Railway Station";
        case FeatureCategory::POI_WORSHIP: return "Place of Worship / Temple";
        case FeatureCategory::POI_TOURISM: return "Tourist Attraction";
        case FeatureCategory::POI_FUEL: return "Fuel Station";
        case FeatureCategory::POI_BANK: return "Bank / ATM";
        case FeatureCategory::POI_HOTEL: return "Hotel / Lodging";
        case FeatureCategory::POI_RESTAURANT: return "Restaurant";
        default: return "Geographic Feature";
    }
}

inline const char* getCategoryIconEmoji(FeatureCategory cat) {
    switch (cat) {
        case FeatureCategory::PLACE_CITY: return "🏙️";
        case FeatureCategory::PLACE_TOWN: return "🏘️";
        case FeatureCategory::PLACE_VILLAGE: return "🏡";
        case FeatureCategory::HIGHWAY_MOTORWAY:
        case FeatureCategory::HIGHWAY_TRUNK:
        case FeatureCategory::HIGHWAY_PRIMARY: return "🛣️";
        case FeatureCategory::HIGHWAY_SECONDARY:
        case FeatureCategory::HIGHWAY_TERTIARY:
        case FeatureCategory::HIGHWAY_RESIDENTIAL: return "🚗";
        case FeatureCategory::RAILWAY_MAIN: return "🚆";
        case FeatureCategory::AEROWAY_RUNWAY:
        case FeatureCategory::POI_AIRPORT: return "✈️";
        case FeatureCategory::WATER_RIVER:
        case FeatureCategory::WATER_LAKE: return "🌊";
        case FeatureCategory::LANDUSE_FOREST:
        case FeatureCategory::LANDUSE_PARK: return "🌳";
        case FeatureCategory::LANDUSE_FARMLAND: return "🌱";
        case FeatureCategory::BUILDING: return "🏢";
        case FeatureCategory::POI_HOSPITAL: return "🏥";
        case FeatureCategory::POI_UNIVERSITY:
        case FeatureCategory::POI_SCHOOL: return "🎓";
        case FeatureCategory::POI_RAILWAY_STATION: return "🚉";
        case FeatureCategory::POI_WORSHIP: return "🛕";
        case FeatureCategory::POI_TOURISM: return "⭐";
        case FeatureCategory::POI_FUEL: return "⛽";
        case FeatureCategory::POI_BANK: return "🏦";
        case FeatureCategory::POI_HOTEL: return "🏨";
        case FeatureCategory::POI_RESTAURANT: return "🍴";
        default: return "📍";
    }
}

} // namespace MapCore
