#include "MapStyle.h"

namespace MapRenderer {

static const RoadStyle& highwayLocalDefault() {
    static RoadStyle def = { QColor(40, 45, 54), QColor(58, 65, 77), 1.5f, 0.8f };
    return def;
}

void MapStyle::setTheme(ThemePreset p) {
    preset = p;

    QFont baseFont("Segoe UI, Helvetica Neue, Ubuntu, Arial, sans-serif");

    if (preset == ThemePreset::GOOGLE_DARK) {
        // Deep Sleek Google Maps Dark / Night Navigation Theme
        backgroundColor = QColor(24, 26, 29); // #181A1D
        gridColor = QColor(50, 55, 65, 80);

        // Water
        waterPolygon = { QColor(19, 39, 56), QColor(28, 59, 84), 0.8f, true }; // #132738
        waterRiver   = { QColor(16, 36, 52), QColor(22, 50, 72), 2.8f, 1.2f };
        waterStream  = { QColor(18, 42, 60), QColor(24, 56, 80), 1.4f, 0.6f };
        waterCanal   = { QColor(16, 36, 52), QColor(22, 50, 72), 2.0f, 0.8f };

        // Landuse & Greenery
        landuseForest      = { QColor(23, 40, 29), QColor(30, 54, 39), 0.5f, false }; // #17281D
        landusePark        = { QColor(27, 51, 36), QColor(35, 66, 47), 0.5f, false }; // #1B3324
        landuseFarmland    = { QColor(34, 41, 32), QColor(43, 51, 40), 0.5f, false }; // #222920
        landuseResidential = { QColor(32, 35, 40), QColor(40, 44, 52), 0.5f, false }; // #202328

        // Roads (Crisp Night Glowing Road Network)
        highwayMotorway     = { QColor(140, 59, 0),   QColor(255, 122, 0),  4.5f, 2.0f }; // Glowing Amber Orange #FF7A00
        highwayTrunk        = { QColor(140, 59, 0),   QColor(255, 138, 30), 4.0f, 1.8f };
        highwayPrimary      = { QColor(135, 96, 0),   QColor(255, 193, 7),  3.4f, 1.5f }; // Gold / Amber #FFC107
        highwaySecondary    = { QColor(47, 53, 66),   QColor(87, 96, 111),  2.8f, 1.2f }; // Slate Gray #57606F
        highwayTertiary     = { QColor(42, 48, 60),   QColor(71, 79, 93),   2.2f, 1.0f };
        highwayResidential  = { QColor(34, 38, 46),   QColor(58, 65, 77),   1.8f, 0.8f };
        highwayUnclassified = { QColor(34, 38, 46),   QColor(58, 65, 77),   1.6f, 0.8f };
        highwayService      = { QColor(28, 31, 38),   QColor(48, 54, 66),   1.2f, 0.5f };
        highwayTrack        = { QColor(42, 38, 32),   QColor(62, 56, 48),   1.0f, 0.5f, Qt::DashLine };
        highwayPath         = { QColor(36, 34, 32),   QColor(54, 51, 48),   0.8f, 0.4f, Qt::DotLine };

        // Rail & Air
        railway = { QColor(32, 36, 43), QColor(112, 123, 140), 1.8f, 0.8f, Qt::DashLine };
        aerowayRunway = { QColor(56, 64, 78), QColor(76, 86, 106), 1.0f, true };

        // Buildings
        building = { QColor(39, 44, 52), QColor(54, 61, 72), 0.7f, true };
        buildingShadow = QColor(0, 0, 0, 100);

        // Boundary
        boundaryState = { QColor(95, 104, 120), QColor(138, 150, 168), 1.8f, 0.0f, Qt::DashDotLine };

        // Labels
        labelCity.textColor = QColor(241, 243, 244);   // #F1F3F4
        labelCity.haloColor = QColor(15, 17, 20, 240);
        labelCity.haloRadius = 3.0f;
        labelCity.font = baseFont;
        labelCity.font.setPointSize(12);
        labelCity.font.setBold(true);

        labelTown.textColor = QColor(210, 214, 220);   // #D2D6DC
        labelTown.haloColor = QColor(15, 17, 20, 230);
        labelTown.haloRadius = 2.5f;
        labelTown.font = baseFont;
        labelTown.font.setPointSize(10);
        labelTown.font.setBold(true);

        labelVillage.textColor = QColor(154, 160, 166); // #9AA0A6
        labelVillage.haloColor = QColor(15, 17, 20, 220);
        labelVillage.haloRadius = 2.0f;
        labelVillage.font = baseFont;
        labelVillage.font.setPointSize(8);

        labelWaterway.textColor = QColor(100, 181, 246); // Light Blue #64B5F6
        labelWaterway.haloColor = QColor(10, 20, 30, 230);
        labelWaterway.haloRadius = 2.0f;
        labelWaterway.font = baseFont;
        labelWaterway.font.setPointSize(9);
        labelWaterway.font.setItalic(true);

        labelHighway.textColor = QColor(255, 213, 79);  // #FFD54F
        labelHighway.haloColor = QColor(25, 20, 10, 230);
        labelHighway.haloRadius = 2.0f;
        labelHighway.font = baseFont;
        labelHighway.font.setPointSize(8);
        labelHighway.font.setBold(true);

        labelPoi.textColor = QColor(138, 180, 248);     // Google Blue #8AB4F8
        labelPoi.haloColor = QColor(15, 17, 20, 240);
        labelPoi.haloRadius = 2.0f;
        labelPoi.font = baseFont;
        labelPoi.font.setPointSize(8);
        labelPoi.font.setBold(true);

        // Overlays
        selectionPinColor = QColor(242, 139, 130);     // Soft Red #F28B82
        selectionRingColor = QColor(138, 180, 248, 180);
        measureLineColor = QColor(138, 180, 248);
        measurePointColor = QColor(255, 255, 255);

    } else if (preset == ThemePreset::GOOGLE_LIGHT) {
        // Standard Light
        backgroundColor = QColor(244, 243, 240);
        gridColor = QColor(220, 220, 220, 100);

        waterPolygon = { QColor(170, 211, 223), QColor(148, 189, 206), 0.8f, true };
        waterRiver   = { QColor(148, 189, 206), QColor(170, 211, 223), 2.5f, 1.0f };
        waterStream  = { QColor(165, 200, 215), QColor(180, 220, 230), 1.2f, 0.5f };
        waterCanal   = { QColor(148, 189, 206), QColor(170, 211, 223), 1.8f, 0.8f };

        landuseForest      = { QColor(206, 232, 206), QColor(180, 215, 180), 0.5f, false };
        landusePark        = { QColor(212, 241, 214), QColor(190, 225, 195), 0.5f, false };
        landuseFarmland    = { QColor(235, 240, 230), QColor(220, 225, 215), 0.5f, false };
        landuseResidential = { QColor(234, 230, 225), QColor(220, 215, 210), 0.5f, false };

        highwayMotorway     = { QColor(212, 130, 43),  QColor(255, 179, 71),  4.5f, 2.0f };
        highwayTrunk        = { QColor(212, 130, 43),  QColor(255, 185, 80),  4.0f, 1.8f };
        highwayPrimary      = { QColor(223, 181, 72),  QColor(254, 213, 101), 3.2f, 1.5f };
        highwaySecondary    = { QColor(197, 197, 197), QColor(255, 255, 255), 2.6f, 1.2f };
        highwayTertiary     = { QColor(210, 210, 210), QColor(255, 255, 255), 2.2f, 1.0f };
        highwayResidential  = { QColor(225, 225, 225), QColor(255, 255, 255), 1.8f, 0.8f };
        highwayUnclassified = { QColor(225, 225, 225), QColor(255, 255, 255), 1.6f, 0.8f };
        highwayService      = { QColor(235, 235, 235), QColor(255, 255, 255), 1.2f, 0.5f };
        highwayTrack        = { QColor(210, 195, 175), QColor(240, 230, 215), 1.0f, 0.5f, Qt::DashLine };
        highwayPath         = { QColor(200, 190, 180), QColor(230, 220, 210), 0.8f, 0.4f, Qt::DotLine };

        railway = { QColor(140, 140, 140), QColor(255, 255, 255), 1.8f, 0.8f, Qt::DashLine };
        aerowayRunway = { QColor(208, 214, 220), QColor(175, 185, 195), 1.0f, true };

        building = { QColor(217, 208, 201), QColor(197, 188, 177), 0.6f, true };
        buildingShadow = QColor(0, 0, 0, 25);
        boundaryState = { QColor(150, 150, 150), QColor(100, 100, 100), 1.5f, 0.0f, Qt::DashDotLine };

        labelCity.textColor = QColor(32, 33, 36);
        labelCity.haloColor = QColor(255, 255, 255, 230);
        labelCity.haloRadius = 3.0f;
        labelCity.font = baseFont;
        labelCity.font.setPointSize(12);
        labelCity.font.setBold(true);

        labelTown.textColor = QColor(60, 64, 67);
        labelTown.haloColor = QColor(255, 255, 255, 220);
        labelTown.haloRadius = 2.5f;
        labelTown.font = baseFont;
        labelTown.font.setPointSize(10);
        labelTown.font.setBold(true);

        labelVillage.textColor = QColor(95, 99, 104);
        labelVillage.haloColor = QColor(255, 255, 255, 210);
        labelVillage.haloRadius = 2.0f;
        labelVillage.font = baseFont;
        labelVillage.font.setPointSize(8);

        labelWaterway.textColor = QColor(41, 105, 130);
        labelWaterway.haloColor = QColor(235, 246, 251, 220);
        labelWaterway.haloRadius = 2.0f;
        labelWaterway.font = baseFont;
        labelWaterway.font.setPointSize(9);
        labelWaterway.font.setItalic(true);

        labelHighway.textColor = QColor(80, 50, 20);
        labelHighway.haloColor = QColor(255, 245, 220, 220);
        labelHighway.haloRadius = 2.0f;
        labelHighway.font = baseFont;
        labelHighway.font.setPointSize(8);
        labelHighway.font.setBold(true);

        labelPoi.textColor = QColor(26, 115, 232);
        labelPoi.haloColor = QColor(255, 255, 255, 230);
        labelPoi.haloRadius = 2.0f;
        labelPoi.font = baseFont;
        labelPoi.font.setPointSize(8);
        labelPoi.font.setBold(true);

        selectionPinColor = QColor(234, 67, 53);
        selectionRingColor = QColor(66, 133, 244, 180);
        measureLineColor = QColor(26, 115, 232);
        measurePointColor = QColor(255, 255, 255);

    } else if (preset == ThemePreset::TERRAIN_NATURE) {
        backgroundColor = QColor(238, 235, 225);
        gridColor = QColor(210, 205, 195, 100);

        waterPolygon = { QColor(135, 195, 220), QColor(110, 175, 200), 1.0f, true };
        waterRiver   = { QColor(110, 175, 200), QColor(135, 195, 220), 3.0f, 1.2f };
        waterStream  = { QColor(125, 185, 210), QColor(150, 205, 230), 1.5f, 0.6f };
        waterCanal   = { QColor(110, 175, 200), QColor(135, 195, 220), 2.0f, 0.8f };

        landuseForest      = { QColor(178, 222, 178), QColor(150, 200, 150), 0.5f, false };
        landusePark        = { QColor(195, 235, 195), QColor(170, 215, 170), 0.5f, false };
        landuseFarmland    = { QColor(225, 236, 215), QColor(205, 220, 195), 0.5f, false };
        landuseResidential = { QColor(228, 220, 210), QColor(210, 200, 190), 0.5f, false };

        highwayMotorway     = { QColor(190, 110, 30),  QColor(245, 160, 50),  4.5f, 2.0f };
        highwayTrunk        = { QColor(190, 110, 30),  QColor(245, 165, 60),  4.0f, 1.8f };
        highwayPrimary      = { QColor(200, 160, 50),  QColor(250, 210, 80),  3.2f, 1.5f };
        highwaySecondary    = { QColor(180, 180, 180), QColor(255, 255, 255), 2.6f, 1.2f };
        highwayTertiary     = { QColor(195, 195, 195), QColor(255, 255, 255), 2.2f, 1.0f };
        highwayResidential  = { QColor(210, 210, 210), QColor(255, 255, 255), 1.8f, 0.8f };
        highwayUnclassified = { QColor(210, 210, 210), QColor(255, 255, 255), 1.6f, 0.8f };
        highwayService      = { QColor(220, 220, 220), QColor(255, 255, 255), 1.2f, 0.5f };
        highwayTrack        = { QColor(190, 175, 150), QColor(225, 215, 195), 1.0f, 0.5f, Qt::DashLine };
        highwayPath         = { QColor(180, 170, 155), QColor(215, 205, 190), 0.8f, 0.4f, Qt::DotLine };

        railway = { QColor(110, 110, 110), QColor(255, 255, 255), 1.8f, 0.8f, Qt::DashLine };
        aerowayRunway = { QColor(195, 202, 210), QColor(160, 170, 180), 1.0f, true };
        building = { QColor(210, 200, 190), QColor(185, 175, 165), 0.6f, true };
        buildingShadow = QColor(0, 0, 0, 30);
        boundaryState = { QColor(130, 130, 130), QColor(90, 90, 90), 1.5f, 0.0f, Qt::DashDotLine };

        labelCity.textColor = QColor(25, 40, 25);
        labelCity.haloColor = QColor(255, 255, 255, 240);
        labelCity.haloRadius = 3.0f;
        labelCity.font = baseFont;
        labelCity.font.setPointSize(12);
        labelCity.font.setBold(true);

        labelTown.textColor = QColor(45, 60, 45);
        labelTown.haloColor = QColor(255, 255, 255, 230);
        labelTown.haloRadius = 2.5f;
        labelTown.font = baseFont;
        labelTown.font.setPointSize(10);
        labelTown.font.setBold(true);

        labelVillage.textColor = QColor(80, 95, 80);
        labelVillage.haloColor = QColor(255, 255, 255, 220);
        labelVillage.haloRadius = 2.0f;
        labelVillage.font = baseFont;
        labelVillage.font.setPointSize(8);

        labelWaterway.textColor = QColor(20, 90, 120);
        labelWaterway.haloColor = QColor(230, 245, 255, 230);
        labelWaterway.haloRadius = 2.0f;
        labelWaterway.font = baseFont;
        labelWaterway.font.setPointSize(9);
        labelWaterway.font.setItalic(true);

        labelHighway.textColor = QColor(70, 45, 15);
        labelHighway.haloColor = QColor(255, 245, 220, 220);
        labelHighway.haloRadius = 2.0f;
        labelHighway.font = baseFont;
        labelHighway.font.setPointSize(8);
        labelHighway.font.setBold(true);

        labelPoi.textColor = QColor(30, 120, 70);
        labelPoi.haloColor = QColor(255, 255, 255, 230);
        labelPoi.haloRadius = 2.0f;
        labelPoi.font = baseFont;
        labelPoi.font.setPointSize(8);
        labelPoi.font.setBold(true);

        selectionPinColor = QColor(234, 67, 53);
        selectionRingColor = QColor(52, 168, 83, 180);
        measureLineColor = QColor(52, 168, 83);
        measurePointColor = QColor(255, 255, 255);

    } else { // HIGH_CONTRAST
        backgroundColor = QColor(0, 0, 0);
        gridColor = QColor(80, 80, 80, 150);

        waterPolygon = { QColor(0, 80, 160), QColor(0, 140, 255), 1.0f, true };
        waterRiver   = { QColor(0, 140, 255), QColor(0, 80, 160), 3.0f, 1.2f };
        waterStream  = { QColor(0, 120, 220), QColor(0, 160, 255), 1.5f, 0.6f };
        waterCanal   = { QColor(0, 140, 255), QColor(0, 80, 160), 2.0f, 0.8f };

        landuseForest      = { QColor(0, 60, 20), QColor(0, 120, 40), 0.5f, false };
        landusePark        = { QColor(0, 80, 30), QColor(0, 150, 60), 0.5f, false };
        landuseFarmland    = { QColor(40, 45, 30), QColor(80, 90, 60), 0.5f, false };
        landuseResidential = { QColor(30, 30, 35), QColor(60, 60, 70), 0.5f, false };

        highwayMotorway     = { QColor(200, 60, 0),   QColor(255, 120, 0),   5.0f, 2.2f };
        highwayTrunk        = { QColor(200, 60, 0),   QColor(255, 140, 0),   4.5f, 2.0f };
        highwayPrimary      = { QColor(200, 150, 0),  QColor(255, 210, 0),   3.8f, 1.6f };
        highwaySecondary    = { QColor(60, 70, 85),   QColor(120, 140, 170), 3.0f, 1.4f };
        highwayTertiary     = { QColor(50, 60, 75),   QColor(100, 120, 150), 2.4f, 1.2f };
        highwayResidential  = { QColor(40, 50, 60),   QColor(80, 95, 120),   2.0f, 1.0f };
        highwayUnclassified = { QColor(40, 50, 60),   QColor(80, 95, 120),   1.8f, 1.0f };
        highwayService      = { QColor(30, 40, 50),   QColor(60, 75, 95),    1.4f, 0.8f };
        highwayTrack        = { QColor(60, 50, 40),   QColor(120, 100, 80),  1.2f, 0.6f, Qt::DashLine };
        highwayPath         = { QColor(50, 45, 40),   QColor(100, 90, 80),   1.0f, 0.5f, Qt::DotLine };

        railway = { QColor(80, 80, 90), QColor(200, 200, 220), 2.0f, 1.0f, Qt::DashLine };
        aerowayRunway = { QColor(60, 70, 90), QColor(100, 120, 150), 1.2f, true };
        building = { QColor(45, 50, 60), QColor(80, 90, 110), 0.8f, true };
        buildingShadow = QColor(0, 0, 0, 80);
        boundaryState = { QColor(120, 130, 150), QColor(200, 210, 230), 2.0f, 0.0f, Qt::DashDotLine };

        labelCity.textColor = QColor(255, 255, 255);
        labelCity.haloColor = QColor(0, 0, 0, 255);
        labelCity.haloRadius = 3.5f;
        labelCity.font = baseFont;
        labelCity.font.setPointSize(13);
        labelCity.font.setBold(true);

        labelTown.textColor = QColor(230, 230, 230);
        labelTown.haloColor = QColor(0, 0, 0, 240);
        labelTown.haloRadius = 3.0f;
        labelTown.font = baseFont;
        labelTown.font.setPointSize(11);
        labelTown.font.setBold(true);

        labelVillage.textColor = QColor(180, 180, 180);
        labelVillage.haloColor = QColor(0, 0, 0, 230);
        labelVillage.haloRadius = 2.5f;
        labelVillage.font = baseFont;
        labelVillage.font.setPointSize(9);

        labelWaterway.textColor = QColor(100, 200, 255);
        labelWaterway.haloColor = QColor(0, 0, 0, 240);
        labelWaterway.haloRadius = 2.5f;
        labelWaterway.font = baseFont;
        labelWaterway.font.setPointSize(10);
        labelWaterway.font.setItalic(true);

        labelHighway.textColor = QColor(255, 200, 80);
        labelHighway.haloColor = QColor(0, 0, 0, 240);
        labelHighway.haloRadius = 2.5f;
        labelHighway.font = baseFont;
        labelHighway.font.setPointSize(9);
        labelHighway.font.setBold(true);

        labelPoi.textColor = QColor(120, 190, 255);
        labelPoi.haloColor = QColor(0, 0, 0, 240);
        labelPoi.haloRadius = 2.5f;
        labelPoi.font = baseFont;
        labelPoi.font.setPointSize(9);
        labelPoi.font.setBold(true);

        selectionPinColor = QColor(255, 50, 50);
        selectionRingColor = QColor(0, 150, 255, 200);
        measureLineColor = QColor(0, 150, 255);
        measurePointColor = QColor(255, 255, 255);
    }
}

const RoadStyle& MapStyle::getRoadStyle(MapCore::FeatureCategory cat) const {
    using namespace MapCore;
    switch (cat) {
        case FeatureCategory::HIGHWAY_MOTORWAY: return highwayMotorway;
        case FeatureCategory::HIGHWAY_TRUNK: return highwayTrunk;
        case FeatureCategory::HIGHWAY_PRIMARY: return highwayPrimary;
        case FeatureCategory::HIGHWAY_SECONDARY: return highwaySecondary;
        case FeatureCategory::HIGHWAY_TERTIARY: return highwayTertiary;
        case FeatureCategory::HIGHWAY_RESIDENTIAL: return highwayResidential;
        case FeatureCategory::HIGHWAY_UNCLASSIFIED: return highwayUnclassified;
        case FeatureCategory::HIGHWAY_SERVICE: return highwayService;
        case FeatureCategory::HIGHWAY_TRACK: return highwayTrack;
        case FeatureCategory::HIGHWAY_PATH: return highwayPath;
        case FeatureCategory::RAILWAY_MAIN: return railway;
        case FeatureCategory::WATER_RIVER: return waterRiver;
        case FeatureCategory::WATER_STREAM: return waterStream;
        case FeatureCategory::WATER_CANAL: return waterCanal;
        case FeatureCategory::BOUNDARY_STATE:
        case FeatureCategory::BOUNDARY_DISTRICT: return boundaryState;
        default: return highwayLocalDefault();
    }
}

const PolygonStyle& MapStyle::getPolygonStyle(MapCore::FeatureCategory cat) const {
    using namespace MapCore;
    switch (cat) {
        case FeatureCategory::WATER_OCEAN:
        case FeatureCategory::WATER_LAKE:
        case FeatureCategory::WATER_RIVER: return waterPolygon;
        case FeatureCategory::LANDUSE_FOREST:
        case FeatureCategory::LANDUSE_NATURE_RESERVE: return landuseForest;
        case FeatureCategory::LANDUSE_PARK: return landusePark;
        case FeatureCategory::LANDUSE_FARMLAND: return landuseFarmland;
        case FeatureCategory::LANDUSE_RESIDENTIAL:
        case FeatureCategory::LANDUSE_COMMERCIAL:
        case FeatureCategory::LANDUSE_INDUSTRIAL: return landuseResidential;
        case FeatureCategory::BUILDING: return building;
        case FeatureCategory::AEROWAY_RUNWAY: return aerowayRunway;
        default: return landuseResidential;
    }
}

const TextStyle& MapStyle::getTextStyle(MapCore::FeatureCategory cat) const {
    using namespace MapCore;
    switch (cat) {
        case FeatureCategory::PLACE_STATE:
        case FeatureCategory::PLACE_CITY: return labelCity;
        case FeatureCategory::PLACE_TOWN: return labelTown;
        case FeatureCategory::PLACE_SUBURB:
        case FeatureCategory::PLACE_VILLAGE:
        case FeatureCategory::PLACE_HAMLET:
        case FeatureCategory::PLACE_LOCALITY: return labelVillage;
        case FeatureCategory::WATER_RIVER:
        case FeatureCategory::WATER_LAKE: return labelWaterway;
        case FeatureCategory::HIGHWAY_MOTORWAY:
        case FeatureCategory::HIGHWAY_TRUNK:
        case FeatureCategory::HIGHWAY_PRIMARY: return labelHighway;
        default: return labelPoi;
    }
}

} // namespace MapRenderer
