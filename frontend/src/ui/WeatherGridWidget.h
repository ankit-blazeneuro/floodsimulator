#pragma once

#include <QWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QTimer>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <vector>
#include <memory>
#include "../core/TileCacheManager.h"
#include "../core/WeatherForecastManager.h"
#include "WeatherHudWidget.h"
#include "TimelineWidget.h"

namespace MapUI {

enum class WeatherGridMetric {
    TemperatureHeatmap = 0,    // 🌡️ Thermal & Temperature Heatmap
    PrecipitationRadar = 1,    // 🌧️ Precipitation & Doppler Radar
    WindStreamlines = 2,       // 💨 Wind Velocity & Aerodynamic Streamlines
    CloudSatellite = 3,        // ☁️ Cloud Cover & Multi-Tier Satellite
    RelativeHumidity = 4,      // 💧 Relative Humidity & Moisture Vapor
    SevereRiskComposite = 5    // ⚡ Severe Weather & Hazard Risk Composite
};

class WeatherGridWidget;

// A single cell in the 6-grid layout
class WeatherGridCellWidget : public QWidget {
    Q_OBJECT

public:
    explicit WeatherGridCellWidget(WeatherGridMetric metric, WeatherGridWidget* parentDashboard, QWidget* parent = nullptr);
    ~WeatherGridCellWidget() override = default;

    WeatherGridMetric getMetric() const { return metricType; }
    void setForecast(const MapCore::WeatherForecastData& forecast, int hourIndex);
    void setHourIndex(int hourIndex);

    void setCenter(double lat, double lon);
    void setZoom(int zoom);
    double getCenterLat() const { return centerLat; }
    double getCenterLon() const { return centerLon; }
    int getZoom() const { return zoomLevel; }

    void setTileProvider(MapCore::OnlineTileProvider provider);
    void setAnimPhase(int phase);
    void setMaximized(bool max);
    void setSelected(bool sel);
    bool isSelected() const { return isSelectedState; }

signals:
    void viewportChanged(double lat, double lon, int zoom, WeatherGridCellWidget* source);
    void locationClicked(double lat, double lon);
    void toggleMaximizeRequested(WeatherGridCellWidget* cell);
    void cellSelected(WeatherGridCellWidget* cell);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    void setupHeaderAndFooter();
    void updateMetricBadge();
    void renderCenterReticle(QPainter& p);

    // Map math
    double lonToTileX(double lon, int zoom) const;
    double latToTileY(double lat, int zoom) const;
    double tileXToLon(double x, int zoom) const;
    double tileYToLat(double y, int zoom) const;
    QPointF geoToScreen(double lat, double lon) const;
    double screenToLat(double screenY) const;
    double screenToLon(double screenX) const;

    // Specialized Renderers for the 6 Grids
    void renderTemperatureHeatmap(QPainter& p);
    void renderPrecipitationRadar(QPainter& p);
    void renderWindStreamlines(QPainter& p);
    void renderCloudSatellite(QPainter& p);
    void renderRelativeHumidity(QPainter& p);
    void renderSevereRiskComposite(QPainter& p);

    WeatherGridMetric metricType;
    WeatherGridWidget* dashboard;

    // Viewport camera
    double centerLat = 26.1445; // Default Guwahati, Assam
    double centerLon = 91.7362;
    int zoomLevel = 7;

    // Dragging state
    bool isDragging = false;
    QPoint lastMousePos;
    QPoint pressMousePos;

    // Weather state
    MapCore::WeatherForecastData currentForecast;
    int currentHourIndex = 0;
    int animPhase = 0;
    bool isMaximizedState = false;
    bool isSelectedState = false;

    MapCore::OnlineTileProvider currentProvider = MapCore::OnlineTileProvider::OpenStreetMap_Dark;

    // Header UI (Transparent floating over map)
    QWidget* headerBar;
    StatusDotWidget* statusDot;
    QLabel* lblTitle;
    QPushButton* btnMaximize;

    // Footer UI (Transparent floating over map)
    QWidget* footerBar;
    QLabel* lblLegendText;
};

// Main 6-Grid Weather Forecast Workspace
class WeatherGridWidget : public QWidget {
    Q_OBJECT

public:
    explicit WeatherGridWidget(QWidget* parent = nullptr);
    ~WeatherGridWidget() override = default;

    void fetchWeatherForLocation(double lat, double lon, const QString& locationName = "");
    void setTimeHour(int hourIndex);
    void setTileProvider(MapCore::OnlineTileProvider provider);
    void setDarkMode(bool isDark);
    void setViewport(double lat, double lon, int zoom);
    double getCenterLat() const { return sharedLat; }
    double getCenterLon() const { return sharedLon; }
    int getZoom() const { return sharedZoom; }

    WeatherHudWidget* getSidebar() const { return sidebar; }
    TimelineWidget* getTimeline() const { return weatherTimeline; }

    void setActive(bool active);

public slots:
    void resetToAssam();
    void resetToIndia();
    void setSyncEnabled(bool sync);
    void zoomIn();
    void zoomOut();

signals:
    void forecastUpdated(const MapCore::WeatherForecastData& data);
    void viewportChanged(double lat, double lon, int zoom);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onCellViewportChanged(double lat, double lon, int zoom, WeatherGridCellWidget* source);
    void onCellLocationClicked(double lat, double lon);
    void onToggleMaximizeCell(WeatherGridCellWidget* cell);
    void onForecastReceived(const MapCore::WeatherForecastData& data);

private:
    void setupUi();
    void setupGridCells();
    void applyDefaultSplitterSizes();

    MapCore::WeatherForecastManager* forecastManager;

    // Viewport synchronization - Matching Simulation Screen Default (Full India View: 22.0° N, 79.0° E, Zoom 5)
    bool syncViewports = true;
    double sharedLat = 22.0;
    double sharedLon = 79.0;
    int sharedZoom = 5;
    MapCore::OnlineTileProvider sharedProvider = MapCore::OnlineTileProvider::OpenStreetMap_Dark;

    // Splitters & Widgets
    QSplitter* hSplitter;               // Horizontal: [ Left: sidebar | Right: vSplitter ]
    WeatherHudWidget* sidebar;          // Resizable Left Sidebar
    QSplitter* vSplitter;               // Vertical:   [ Top: gridContainer | Bottom: weatherTimeline ]
    TimelineWidget* weatherTimeline;    // Bottom Timeline (same as simulation)

    // 6 Grid Cells
    std::vector<WeatherGridCellWidget*> gridCells;
    QGridLayout* gridLayout;
    QWidget* gridContainer;
    WeatherGridCellWidget* maximizedCell = nullptr;

    QTimer* animTimer;
    int currentHour = 0;
    int animPhase = 0;
    bool initialSplitterSizesSet = false;

    MapCore::WeatherForecastData currentForecast;
};

} // namespace MapUI
