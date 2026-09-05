#pragma once

#include "OnlineTileWidget.h"
#include "../core/ElevationModel.h"
#include <QImage>
#include <QTimer>

namespace MapUI {

class SeaLevelTileWidget : public OnlineTileWidget {
    Q_OBJECT

private:
    double seaLevelRise = 0.0; // Simulated Sea Level Rise in meters (0.0 to 20.0)
    MapCore::HeatMapPalette currentPalette = MapCore::HeatMapPalette::SeaLevelInundation;
    float heatOpacity = 0.70f;
    bool showContours = true;
    bool showSurgeRipples = true;
    bool showHotspots = true;
    bool heatmapEnabled = true;

    // Rising Tide Simulation Animation
    bool isSimulatingRise = false;
    double riseDirection = 1.0;
    QTimer* animTimer = nullptr;
    int animPhase = 0;

    // Mouse probe
    QPoint liveCursorPos;
    bool hasCursor = false;

public:
    explicit SeaLevelTileWidget(QWidget* parent = nullptr);

    void setSeaLevelRise(double meters);
    double getSeaLevelRise() const { return seaLevelRise; }

    void setPalette(MapCore::HeatMapPalette pal);
    MapCore::HeatMapPalette getPalette() const { return currentPalette; }

    void setHeatOpacity(float op);
    float getHeatOpacity() const { return heatOpacity; }

    void setShowContours(bool show);
    bool getShowContours() const { return showContours; }

    void setShowSurgeRipples(bool show);
    bool getShowSurgeRipples() const { return showSurgeRipples; }

    void setShowHotspots(bool show);
    bool getShowHotspots() const { return showHotspots; }

    void setHeatmapEnabled(bool enabled);
    bool isHeatmapEnabled() const { return heatmapEnabled; }

    void startRiseSimulation();
    void pauseRiseSimulation();
    void resetRiseSimulation();
    bool isSimulating() const { return isSimulatingRise; }

    // Area and population calculation for current viewport
    double calculateInundatedAreaKm2() const;
    double calculateVulnerablePopulationMillions() const;

signals:
    void seaLevelRiseChanged(double meters);
    void simulationStateChanged(bool isRunning);
    void cursorElevationProbe(double lat, double lon, double elevMSL, double clearanceM);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
};

} // namespace MapUI
