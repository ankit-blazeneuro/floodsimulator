#pragma once

#include <QWidget>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "SeaLevelTileWidget.h"
#include "NavigationControls.h"

namespace MapUI {

class SeaLevelHeatMapWidget : public QWidget {
    Q_OBJECT

private:
    SeaLevelTileWidget* mapWidget;

    // Floating UI Panels
    QWidget* controlCard;
    QWidget* presetsBar;
    QWidget* probeBadge;
    QWidget* legendBar;
    NavigationControls* navControls;
    ScaleBar* scaleBar;
    QWidget* statusHud;

    // Controls
    QLabel* lblSeaLevelVal;
    QLabel* lblRiskTag;
    QSlider* sldSeaLevel;
    QPushButton* btnPlaySim;
    QPushButton* btnResetSim;

    QComboBox* cmbPalette;
    QSlider* sldOpacity;
    QCheckBox* chkContours;
    QCheckBox* chkRipples;
    QCheckBox* chkHotspots;
    QComboBox* cmbBaseMap;

    QLabel* lblInundatedArea;
    QLabel* lblVulnerablePop;

    // Probe HUD
    QLabel* lblProbeCoords;
    QLabel* lblProbeElev;
    QLabel* lblProbeClearance;
    QLabel* lblProbeRisk;

    // Legend
    QWidget* legendGradient;
    QLabel* lblLegendTitle;

    // Status HUD
    QLabel* lblStatusCoords;
    QLabel* lblStatusZoom;
    QLabel* lblStatusFps;
    QLabel* lblStatusMode;

public:
    explicit SeaLevelHeatMapWidget(QWidget* parent = nullptr);
    ~SeaLevelHeatMapWidget() override = default;

    void setViewport(double lat, double lon, int zoom);
    double getCenterLat() const { return mapWidget->getCenterLat(); }
    double getCenterLon() const { return mapWidget->getCenterLon(); }
    int getZoom() const { return mapWidget->getZoom(); }

    void zoomIn() { mapWidget->zoomIn(); }
    void zoomOut() { mapWidget->zoomOut(); }
    void fitIndia() { mapWidget->fitIndia(); }
    void setDarkMode(bool isDark);
    void setTool(MapTool tool) { if (mapWidget) mapWidget->setTool(tool); }

    void updateMetrics();

signals:
    void viewportChanged(double lat, double lon, int zoom);
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void setupUi();
    void setupConnections();
    void updateFloatingPositions();
    void updateLegendGradient();
};

} // namespace MapUI
