#pragma once

#include <QWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPixmap>
#include <QTimer>
#include <QPoint>
#include <QPointF>
#include <QHash>
#include <QCache>
#include <QString>
#include <vector>
#include <cmath>
#include "../core/DamManager.h"
#include "../core/DamFloodSimulation.h"
#include "MapTool.h"

namespace MapUI {

// Represents a single map tile identified by (provider, zoom, x, y)
struct TileKey {
    int provider;
    int zoom;
    int x;
    int y;

    bool operator==(const TileKey& o) const {
        return provider == o.provider && zoom == o.zoom && x == o.x && y == o.y;
    }
};

inline uint qHash(const TileKey& key, uint seed = 0) {
    return ::qHash(key.provider, seed) ^ ::qHash(key.zoom, seed) ^ ::qHash(key.x, seed) ^ ::qHash(key.y, seed);
}

enum class OnlineTileProvider {
    OpenStreetMap_Standard = 0,
    OpenStreetMap_DE = 1,
    OpenStreetMap_Voyager = 2,
    OpenStreetMap_Dark = 3
};

class OnlineTileWidget : public QWidget {
    Q_OBJECT

private:
    QNetworkAccessManager* networkManager;

    // In-memory tile cache (max ~500 tiles ≈ ~100 MB)
    QCache<TileKey, QPixmap> tileCache;

    // Tiles currently being downloaded
    QSet<TileKey> pendingTiles;

    // Camera state in geo coordinates (WGS84) - Default: Full India View
    double centerLat = 22.0;
    double centerLon = 79.0;
    int zoomLevel = 5;

    // Mouse dragging state
    bool isDragging = false;
    QPoint lastMousePos;

    OnlineTileProvider currentProvider = OnlineTileProvider::OpenStreetMap_Standard;

    // Zoom Sensitivity & Accumulator for smooth two-finger touchpad / mouse zoom
    double zoomSensitivity = 0.5;
    double wheelAccumulator = 0.0;
    bool anchorZoomToCursor = true;
    bool invertScroll = false;

    // Dams Layer
    const MapCore::DamManager* damManager = nullptr;
    bool showDams = true;

    // Active Tool & Mode
    MapTool currentTool = MapTool::Move;

    // Measurement Tool (Ruler)
    bool measureMode = false;
    std::vector<QPointF> measurePoints; // stored as (lat, lon)
    QPoint liveMousePos;
    bool hasLiveMouse = false;
    QPoint pressMousePos;

    // Box Selection (Select Tool)
    bool isBoxSelecting = false;
    QPoint boxSelectStart;
    QPoint boxSelectCurrent;
    std::vector<const MapCore::DamPoint*> selectedDams;

    // 360° Map Rotation (Rotate Tool)
    double rotationAngle = 0.0;
    bool isRotating = false;
    double lastRotationMouseAngle = 0.0;

    // 60-Minute Hydrodynamic Dam Flood Simulation & River Flow Animation
    MapCore::FloodSimulationState floodSimulation;
    QTimer* flowAnimTimer = nullptr;
    int flowAnimPhase = 0;

    static constexpr int TILE_SIZE = 256;

public:
    explicit OnlineTileWidget(QWidget* parent = nullptr);

    void setDamManager(const MapCore::DamManager* mgr) { damManager = mgr; update(); }
    void setShowDams(bool show) { showDams = show; update(); }
    bool getShowDams() const { return showDams; }

    void setTool(MapTool tool);
    MapTool getTool() const { return currentTool; }

    void setMeasureMode(bool active);
    bool isMeasureMode() const { return measureMode; }
    void clearMeasure();
    void clearBoxSelection();
    const std::vector<const MapCore::DamPoint*>& getSelectedDams() const { return selectedDams; }

    void setFloodSimulation(const MapCore::FloodSimulationState& sim);
    void updateFloodSimulationMinute(int minute);
    void clearFloodSimulation();
    const MapCore::FloodSimulationState& getFloodSimulation() const { return floodSimulation; }

    void setRotation(double degrees);
    double getRotation() const { return rotationAngle; }
    void resetRotation();

    QPointF unrotatePoint(const QPointF& pt) const;
    QPointF rotatePoint(const QPointF& pt) const;

    double screenToLon(double screenX, double screenY) const;
    double screenToLat(double screenX, double screenY) const;
    double screenToLon(double screenX) const { return screenToLon(screenX, height() / 2.0); }
    double screenToLat(double screenY) const { return screenToLat(width() / 2.0, screenY); }
    QPointF geoToScreen(double lat, double lon) const;
    static double haversineDistanceM(double lat1, double lon1, double lat2, double lon2);

    void setCenter(double lat, double lon);
    void setZoom(int z);
    void zoomIn();
    void zoomOut();
    void fitIndia();
    void fitAssam();

    void setTileProvider(OnlineTileProvider provider);
    OnlineTileProvider getTileProvider() const { return currentProvider; }

    void setDarkMode(bool isDark) {
        setTileProvider(isDark ? OnlineTileProvider::OpenStreetMap_Dark : OnlineTileProvider::OpenStreetMap_Standard);
    }
    bool isDarkMode() const {
        return currentProvider == OnlineTileProvider::OpenStreetMap_Dark;
    }

    void setZoomSensitivity(double s) { zoomSensitivity = std::clamp(s, 0.05, 3.0); }
    double getZoomSensitivity() const { return zoomSensitivity; }

    void setAnchorZoomToCursor(bool a) { anchorZoomToCursor = a; }
    bool getAnchorZoomToCursor() const { return anchorZoomToCursor; }

    void setInvertScroll(bool inv) { invertScroll = inv; }
    bool getInvertScroll() const { return invertScroll; }

    void setCacheCapacity(int maxTiles) { tileCache.setMaxCost(maxTiles); }

    double getCenterLat() const { return centerLat; }
    double getCenterLon() const { return centerLon; }
    int getZoom() const { return zoomLevel; }

signals:
    void viewportChanged(double lat, double lon, int zoom);
    void damClicked(const MapCore::DamPoint& dam);
    void measureModeChanged(bool active);
    void contextMenuRequested(const QPoint& globalPos);
    void boxSelectionCompleted(double minLat, double minLon, double maxLat, double maxLon, int count);
    void rotationChanged(double degrees);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    static double lonToTileX(double lon, int zoom);
    static double latToTileY(double lat, int zoom);
    static double tileXToLon(double x, int zoom);
    static double tileYToLat(double y, int zoom);

    QString getPrimaryUrl(OnlineTileProvider provider, int zoom, int x, int y) const;
    QString getFallbackUrl(int zoom, int x, int y) const;

    QPixmap* getTile(int zoom, int x, int y);
    void fetchTile(int zoom, int x, int y, bool isFallback = false);

    void emitViewportChanged();
};

} // namespace MapUI
