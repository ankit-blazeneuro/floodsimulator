#pragma once

#include <QWidget>
#include <QTimer>
#include <QPoint>
#include <QPointF>
#include <vector>
#include "../core/SpatialIndex.h"
#include "../renderer/MapRenderer.h"

namespace MapUI {

class MapWidget : public QWidget {
    Q_OBJECT

private:
    const MapCore::SpatialIndex* spatialIndex = nullptr;
    MapRenderer::MapRenderer renderer;

    // Camera state
    MapCore::Point2D center;
    float zoomLevel = 8.5f;
    float targetZoom = 8.5f;

    // Camera animation (FlyTo)
    bool isAnimatingFlyTo = false;
    MapCore::Point2D flyStartCenter;
    MapCore::Point2D flyEndCenter;
    float flyStartZoom = 8.5f;
    float flyEndZoom = 8.5f;
    qint64 flyStartTime = 0;
    qint64 flyDurationMs = 700;

    // Mouse interaction & Inertia
    bool isDragging = false;
    QPoint lastMousePos;
    QPointF panVelocity;
    QTimer* animTimer;

    // Selection & Measurement
    MapCore::Point2D selectedPos = MapCore::Point2D(-1, -1);
    bool measureMode = false;
    std::vector<MapCore::Point2D> measurePoints;

    // Zoom & Touchpad settings
    double zoomSensitivity = 0.5;
    bool anchorZoomToCursor = true;
    bool invertScroll = false;

    // Performance tracking
    int frameCount = 0;
    qint64 lastFpsTime = 0;
    float currentFps = 60.0f;

public:
    explicit MapWidget(QWidget* parent = nullptr);

    void setSpatialIndex(const MapCore::SpatialIndex* index);
    const MapCore::SpatialIndex* getSpatialIndex() const { return spatialIndex; }

    MapRenderer::MapRenderer& getRenderer() { return renderer; }
    const MapRenderer::MapRenderer& getRenderer() const { return renderer; }

    // Camera control
    void setCenter(const MapCore::Point2D& c);
    MapCore::Point2D getCenter() const { return center; }

    void setZoom(float z);
    float getZoom() const { return zoomLevel; }

    void zoomIn();
    void zoomOut();
    void fitExtent(const MapCore::BoundingBox& box);
    void fitAssam();

    void flyTo(const MapCore::Point2D& targetCenter, float targetZoomLevel, qint64 durationMs = 700);

    // Selection & Measure
    void setSelectedPosition(const MapCore::Point2D& pos);
    void clearSelection();

    void setMeasureMode(bool active);
    bool isMeasureMode() const { return measureMode; }
    void clearMeasure();

    void setZoomSensitivity(double s) { zoomSensitivity = std::clamp(s, 0.05, 3.0); }
    double getZoomSensitivity() const { return zoomSensitivity; }

    void setAnchorZoomToCursor(bool a) { anchorZoomToCursor = a; }
    bool getAnchorZoomToCursor() const { return anchorZoomToCursor; }

    void setInvertScroll(bool inv) { invertScroll = inv; }
    bool getInvertScroll() const { return invertScroll; }

signals:
    void viewportChanged(MapCore::BoundingBox viewBbox, float zoomLevel, MapCore::GeoCoord centerGeo);
    void featureSelected(MapCore::FeatureInfo info);
    void cursorGeoMoved(MapCore::GeoCoord geo, QString hoverText);
    void fpsChanged(float fps);
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onAnimationTick();

private:
    void updateViewportNotification();
    static float easeInOutCubic(float t);
};

} // namespace MapUI
