#pragma once

#include <QColor>
#include <QFont>
#include <string>
#include "../core/MapFeature.h"

namespace MapRenderer {

enum class ThemePreset {
    GOOGLE_DARK = 0,
    GOOGLE_LIGHT,
    TERRAIN_NATURE,
    HIGH_CONTRAST
};

struct RoadStyle {
    QColor casingColor;
    QColor coreColor;
    float baseWidth = 2.0f;
    float casingExtra = 1.5f;
    Qt::PenStyle penStyle = Qt::SolidLine;
};

struct PolygonStyle {
    QColor fillColor;
    QColor outlineColor;
    float outlineWidth = 0.5f;
    bool hasOutline = false;
};

struct TextStyle {
    QColor textColor;
    QColor haloColor;
    float haloRadius = 2.0f;
    QFont font;
    bool bold = false;
    bool italic = false;
};

class MapStyle {
public:
    ThemePreset preset = ThemePreset::GOOGLE_DARK;

    // Background
    QColor backgroundColor;
    QColor gridColor;

    // Water
    PolygonStyle waterPolygon;
    RoadStyle waterRiver;
    RoadStyle waterStream;
    RoadStyle waterCanal;

    // Landuse & Natural
    PolygonStyle landuseForest;
    PolygonStyle landusePark;
    PolygonStyle landuseFarmland;
    PolygonStyle landuseResidential;

    // Roads
    RoadStyle highwayMotorway;
    RoadStyle highwayTrunk;
    RoadStyle highwayPrimary;
    RoadStyle highwaySecondary;
    RoadStyle highwayTertiary;
    RoadStyle highwayResidential;
    RoadStyle highwayUnclassified;
    RoadStyle highwayService;
    RoadStyle highwayTrack;
    RoadStyle highwayPath;

    // Rail & Air
    RoadStyle railway;
    PolygonStyle aerowayRunway;

    // Buildings
    PolygonStyle building;
    QColor buildingShadow;

    // Boundaries
    RoadStyle boundaryState;

    // Labels
    TextStyle labelCity;
    TextStyle labelTown;
    TextStyle labelVillage;
    TextStyle labelWaterway;
    TextStyle labelHighway;
    TextStyle labelPoi;

    // Overlays
    QColor selectionPinColor;
    QColor selectionRingColor;
    QColor measureLineColor;
    QColor measurePointColor;

public:
    MapStyle(ThemePreset p = ThemePreset::GOOGLE_DARK) {
        setTheme(p);
    }

    void setTheme(ThemePreset p);

    const RoadStyle& getRoadStyle(MapCore::FeatureCategory cat) const;
    const PolygonStyle& getPolygonStyle(MapCore::FeatureCategory cat) const;
    const TextStyle& getTextStyle(MapCore::FeatureCategory cat) const;
};

} // namespace MapRenderer
