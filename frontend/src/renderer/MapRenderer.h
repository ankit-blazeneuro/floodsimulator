#pragma once

#include <QPainter>
#include <QPaintDevice>
#include <vector>
#include "../core/SpatialIndex.h"
#include "MapStyle.h"
#include "LabelPlacer.h"

namespace MapRenderer {

struct RenderOptions {
    bool showBuildings = true;
    bool showRoads = true;
    bool showWater = true;
    bool showLanduse = true;
    bool showLabels = true;
    bool showPois = true;
    bool showGrid = false;
};

class MapRenderer {
private:
    MapStyle style;
    RenderOptions options;
    LabelPlacer labelPlacer;

public:
    MapRenderer(ThemePreset preset = ThemePreset::GOOGLE_LIGHT) : style(preset) {}

    void setStyle(const MapStyle& s) { style = s; }
    MapStyle& getStyle() { return style; }
    const MapStyle& getStyle() const { return style; }

    void setOptions(const RenderOptions& opt) { options = opt; }
    RenderOptions& getOptions() { return options; }
    const RenderOptions& getOptions() const { return options; }

    // Coordinate conversion utilities
    static inline double calculateScale(float zoomLevel) {
        return 256.0 * std::pow(2.0, static_cast<double>(zoomLevel));
    }

    static inline QPointF mercatorToScreen(const MapCore::Point2D& pt,
                                          const MapCore::Point2D& center,
                                          float zoomLevel, int width, int height) {
        double scale = calculateScale(zoomLevel);
        qreal sx = (pt.x - center.x) * scale + width * 0.5;
        qreal sy = (pt.y - center.y) * scale + height * 0.5;
        return QPointF(sx, sy);
    }

    static inline MapCore::Point2D screenToMercator(const QPointF& screenPt,
                                                   const MapCore::Point2D& center,
                                                   float zoomLevel, int width, int height) {
        double scale = calculateScale(zoomLevel);
        float mx = static_cast<float>((screenPt.x() - width * 0.5) / scale + center.x);
        float my = static_cast<float>((screenPt.y() - height * 0.5) / scale + center.y);
        return MapCore::Point2D(mx, my);
    }

    static inline MapCore::BoundingBox getViewportMercatorBbox(const MapCore::Point2D& center,
                                                              float zoomLevel, int width, int height) {
        MapCore::Point2D topLeft = screenToMercator(QPointF(0, 0), center, zoomLevel, width, height);
        MapCore::Point2D bottomRight = screenToMercator(QPointF(width, height), center, zoomLevel, width, height);
        return MapCore::BoundingBox(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
    }

    void render(QPainter& painter, const MapCore::SpatialIndex& index,
                const MapCore::Point2D& center, float zoomLevel,
                int width, int height,
                const MapCore::Point2D& selectedPos = MapCore::Point2D(-1, -1),
                const std::vector<MapCore::Point2D>& measurePoints = {});
};

} // namespace MapRenderer
