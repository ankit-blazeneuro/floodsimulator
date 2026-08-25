#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QProgressBar>
#include <QThread>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QStackedWidget>
#include <QSplitter>
#include "../core/SpatialIndex.h"
#include "MapWidget.h"
#include "OnlineTileWidget.h"
#include "SearchBar.h"
#include "PlaceCard.h"
#include "NavigationControls.h"
#include "LayerPanel.h"
#include "MiniMap.h"
#include "SettingsDialog.h"
#include "TimelineWidget.h"
#include "PropertiesPanel.h"
#include "PieMenu.h"
#include "WeatherGridWidget.h"
#include "HelpSupportWidget.h"

namespace MapUI {

class LoadingOverlay : public QWidget {
    Q_OBJECT

private:
    QLabel* lblLogo;
    QLabel* lblStatus;
    QProgressBar* progressBar;

public:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    void setProgress(float progress, const QString& status);
};

enum class AppTheme {
    SystemDefault = 0,
    Dark = 1,
    Light = 2
};

class MainWindow : public QMainWindow {
    Q_OBJECT

private:
    MapCore::SpatialIndex spatialIndex;

    // Map mode
    enum class MapMode { Online, Offline };
    MapMode currentMapMode = MapMode::Online;
    AppTheme currentTheme = AppTheme::SystemDefault;

    // Multi-Screen Root Workspace Stack (0: Simulation Map, 1: Analytics, 2: Weather Forecast, 3: Help & Support)
    QStackedWidget* rootStack;
    QWidget* analyticsScreen;
    WeatherGridWidget* weatherScreen;
    HelpSupportWidget* helpScreen;

    // Resizable Splitter Layout
    QSplitter* mainSplitter;    // Horizontal: [ Left: vSplitter | Right: PropertiesPanel ]
    QSplitter* vSplitter;       // Vertical:   [ Top: MapContainer | Bottom: TimelineWidget ]
    QWidget* mapContainer;
    PropertiesPanel* propertiesPanel;

    // Stacked widget to switch between online/offline map views
    QStackedWidget* mapStack;

    // UI Elements
    MapWidget* mapWidget;           // Offline map (custom QPainter renderer)
    OnlineTileWidget* onlineMap;    // Online map (OSM tile fetcher)
    SearchBar* searchBar;
    PlaceCard* placeCard;
    NavigationControls* navControls;
    ScaleBar* scaleBar;
    LayerPanel* layerPanel;
    QPushButton* btnToggleLayers;
    MiniMap* miniMap;
    QPushButton* btnToggleMiniMap;
    TimelineWidget* timelineWidget; // Fully resizable bottom timeline
    LoadingOverlay* loadingOverlay;
    SettingsDialog* settingsDialog = nullptr;
    PieMenu* pieMenu = nullptr;
    MapTool currentTool = MapTool::Move;

    // Menu Bar
    QMenuBar* appMenuBar;
    QMenu* fileMenu;
    QMenu* viewMenu;
    QMenu* themeMenu;
    QMenu* onlineStylesMenu;
    QMenu* settingsMenu;

    QAction* actionOnline;
    QAction* actionOffline;
    QAction* actionToggleSidebar;
    QAction* actionToggleTimeline;
    QAction* actionToggleTerminal;

    QAction* actionThemeSystem;
    QAction* actionThemeDark;
    QAction* actionThemeLight;

    QAction* actionOsmStandard;
    QAction* actionOsmDe;
    QAction* actionOsmVoyager;
    QAction* actionOsmDark;

    QAction* actionOpenSettings;
    QAction* actionSensLow;
    QAction* actionSensMed;
    QAction* actionSensHigh;

    QPushButton* btnWorkspaceSim;
    QPushButton* btnWorkspaceAnalytics;
    QPushButton* btnWorkspaceWeather;
    QPushButton* btnWorkspaceHelp;

    QAction* actionExit;
    QAction* actionFullscreen;

    // Status HUD
    QWidget* statusHud;
    QLabel* lblCoordinates;
    QLabel* lblZoomLevel;
    QLabel* lblFps;
    QLabel* lblFeatureCount;
    QLabel* lblMapMode;
    QLabel* lblTheme;

    std::string dataPbfPath = "data/assam-latest.osm.pbf";
    std::string cacheFilePath = "data/assam_map.bin";
    std::unique_ptr<MapCore::DamManager> damManager;

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    void startAsyncLoad();
    void applyTheme(AppTheme theme);
    void applyAppSettings(const AppSettings& settings);
    static bool isSystemDarkTheme();

    PieMenu* getPieMenu() const { return pieMenu; }
    MapTool getActiveTool() const { return currentTool; }
    HelpSupportWidget* getHelpScreen() const { return helpScreen; }

public slots:
    void openSettingsDialog();
    void togglePropertiesPanel();
    void toggleTimeline();
    void toggleTerminal();
    void showSimulationScreen();
    void showAnalyticsScreen();
    void showWeatherScreen();
    void showHelpSupportScreen();
    void setActiveTool(MapTool tool);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onMapLoaded();
    void onSearchResultSelected(MapCore::Point2D pos, float targetZoom,
                                QString name, QString detail, MapCore::FeatureCategory category);
    void onFeatureSelected(MapCore::FeatureInfo info);
    void onViewportChanged(MapCore::BoundingBox viewBbox, float zoomLevel, MapCore::GeoCoord centerGeo);
    void onCursorGeoMoved(MapCore::GeoCoord geo, QString hoverText);
    void onFpsChanged(float fps);
    void onCurrentScreenChanged(int index);

    // Map mode switching
    void switchToOnline();
    void switchToOffline();

private:
    void setupUi();
    void setupMenuBar();
    void updateFloatingPositions();
};

} // namespace MapUI
