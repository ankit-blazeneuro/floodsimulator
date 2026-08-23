#include "MainWindow.h"
#include "IconHelper.h"
#include "../core/OsmPbfLoader.h"
#include "../core/MapDataCache.h"
#include <QtConcurrent/QtConcurrent>
#include <QGraphicsDropShadowEffect>
#include <QResizeEvent>
#include <QApplication>
#include <QStyle>
#include <QProcess>
#include <QWidgetAction>
#include <iostream>

namespace MapUI {

LoadingOverlay::LoadingOverlay(QWidget* parent) : QWidget(parent) {
    setStyleSheet(R"(
        QWidget#loadingCard {
            background-color: #242424;
            border: 1px solid #333333;
            border-radius: 12px;
        }
        QProgressBar {
            border: none;
            border-radius: 3px;
            background-color: #181818;
            height: 6px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #4772B3;
            border-radius: 3px;
        }
    )");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setAlignment(Qt::AlignCenter);

    auto* card = new QWidget(this);
    card->setObjectName("loadingCard");
    card->setFixedSize(360, 190);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(0, 0, 0, 140));
    shadow->setOffset(0, 6);
    card->setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(10);

    lblLogo = new QLabel("Assam & India Flood Simulator", card);
    lblLogo->setAlignment(Qt::AlignCenter);
    lblLogo->setStyleSheet("font-family: 'Segoe UI', Arial, sans-serif; font-size: 16px; font-weight: bold; color: #FFFFFF;");

    auto* lblSub = new QLabel("High Performance GIS & Hydrodynamic Engine", card);
    lblSub->setAlignment(Qt::AlignCenter);
    lblSub->setStyleSheet("font-family: 'Segoe UI', Arial, sans-serif; font-size: 11px; color: #888888;");

    progressBar = new QProgressBar(card);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);

    lblStatus = new QLabel("Initializing mapping environment...", card);
    lblStatus->setAlignment(Qt::AlignCenter);
    lblStatus->setStyleSheet("font-family: 'Segoe UI', Arial, sans-serif; font-size: 11px; color: #CCCCCC;");

    layout->addWidget(lblLogo);
    layout->addWidget(lblSub);
    layout->addSpacing(4);
    layout->addWidget(progressBar);
    layout->addWidget(lblStatus);

    rootLayout->addWidget(card);
}

void LoadingOverlay::setProgress(float progress, const QString& status) {
    progressBar->setValue(static_cast<int>(progress * 100));
    lblStatus->setText(status);
}

bool MainWindow::isSystemDarkTheme() {
    QProcess process;
    process.start("gsettings", {"get", "org.gnome.desktop.interface", "color-scheme"});
    if (process.waitForFinished(200)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        if (output.contains("prefer-dark")) {
            return true;
        } else if (output.contains("prefer-light")) {
            return false;
        }
    }

    QColor winColor = QGuiApplication::palette().color(QPalette::Window);
    if (winColor.isValid()) {
        return winColor.value() < 128;
    }

    return true;
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Assam & India Maps - Flood Simulator");
    setWindowIcon(IconHelper::logo(QColor(212, 212, 216), 32));
    resize(1380, 860);
    setMinimumSize(920, 600);

    setupUi();
    setupMenuBar();

    // Create Settings dialog
    settingsDialog = new SettingsDialog(this);
    connect(settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::applyAppSettings);

    // Apply saved or default settings (Theme = System Default, Mode = Online)
    applyAppSettings(settingsDialog->getSettings());

    // Guarantee default online map view
    switchToOnline();
    onlineMap->fitIndia();

    // Start async offline data load in background
    startAsyncLoad();
}

void MainWindow::applyTheme(AppTheme theme) {
    currentTheme = theme;
    bool isDark = false;

    if (theme == AppTheme::SystemDefault) {
        isDark = isSystemDarkTheme();
        lblTheme->setText(isDark ? "System (Dark)" : "System (Light)");
        if (actionThemeSystem) actionThemeSystem->setChecked(true);
    } else if (theme == AppTheme::Dark) {
        isDark = true;
        lblTheme->setText("Dark");
        if (actionThemeDark) actionThemeDark->setChecked(true);
    } else {
        isDark = false;
        lblTheme->setText("Light");
        if (actionThemeLight) actionThemeLight->setChecked(true);
    }

    // Set online map mode & dark mode based on theme
    onlineMap->setDarkMode(isDark);
    if (isDark) {
        if (actionOsmDark) actionOsmDark->setChecked(true);
    } else {
        if (actionOsmStandard) actionOsmStandard->setChecked(true);
    }

    // Update Offline Map Style
    mapWidget->getRenderer().getStyle().setTheme(isDark ? MapRenderer::ThemePreset::GOOGLE_DARK : MapRenderer::ThemePreset::GOOGLE_LIGHT);
    mapWidget->update();
}

void MainWindow::applyAppSettings(const AppSettings& settings) {
    // 1. Apply zoom sensitivity to both online and offline map views
    onlineMap->setZoomSensitivity(settings.zoomSensitivity);
    onlineMap->setAnchorZoomToCursor(settings.anchorZoomToCursor);
    onlineMap->setInvertScroll(settings.invertScroll);
    onlineMap->setCacheCapacity(settings.tileCacheSize);

    mapWidget->setZoomSensitivity(settings.zoomSensitivity);
    mapWidget->setAnchorZoomToCursor(settings.anchorZoomToCursor);
    mapWidget->setInvertScroll(settings.invertScroll);

    // 2. Apply theme preference
    if (settings.startupTheme == 0) {
        actionThemeSystem->setChecked(true);
        applyTheme(AppTheme::SystemDefault);
    } else if (settings.startupTheme == 1) {
        actionThemeDark->setChecked(true);
        applyTheme(AppTheme::Dark);
    } else {
        actionThemeLight->setChecked(true);
        applyTheme(AppTheme::Light);
    }

    // 3. Apply startup mode (Online / Offline)
    if (settings.startupMode == 0) {
        actionOnline->setChecked(true);
        switchToOnline();
    } else {
        actionOffline->setChecked(true);
        switchToOffline();
    }

    // 4. Apply minimap visibility
    btnToggleMiniMap->setChecked(settings.showMinimap);
    miniMap->setVisible(settings.showMinimap);
    updateFloatingPositions();
}

void MainWindow::openSettingsDialog() {
    if (!settingsDialog) {
        settingsDialog = new SettingsDialog(this);
        connect(settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::applyAppSettings);
    }
    settingsDialog->loadSettings();
    settingsDialog->exec();
}

void MainWindow::togglePropertiesPanel() {
    bool nextVisible = !propertiesPanel->isVisible();
    propertiesPanel->setVisible(nextVisible);
    if (nextVisible) {
        mainSplitter->setSizes({ width() - 300, 300 });
    }
    updateFloatingPositions();
}

void MainWindow::toggleTimeline() {
    bool nextVisible = !timelineWidget->isVisible();
    timelineWidget->setVisible(nextVisible);
    if (nextVisible) {
        vSplitter->setSizes({ vSplitter->height() - 140, 140 });
    }
    updateFloatingPositions();
}

void MainWindow::setupMenuBar() {
    appMenuBar = menuBar();
    appMenuBar->setStyleSheet(R"(
        QMenuBar {
            background-color: #1F1F1F;
            color: #D4D4D8;
            border-bottom: 1px solid #141414;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: 600;
            padding: 2px 4px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 4px 10px;
            border-radius: 3px;
            color: #D4D4D8;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: 600;
        }
        QMenuBar::item:selected {
            background-color: #383838;
            color: #FFFFFF;
        }
        QMenuBar::item:pressed {
            background-color: #4772B3;
            color: #FFFFFF;
        }
        QMenu {
            background-color: #242424;
            color: #E6E6E6;
            border: 1px solid #181818;
            border-radius: 4px;
            padding: 3px 0px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
        }
        QMenu::item {
            padding: 6px 20px 6px 12px;
            color: #E6E6E6;
            font-size: 11px;
            font-weight: 500;
        }
        QMenu::item:selected {
            background-color: #4772B3;
            color: #FFFFFF;
            border-radius: 2px;
        }
        QMenu::separator {
            height: 1px;
            background-color: #333333;
            margin: 3px 6px;
        }
        QMenu::indicator {
            width: 12px;
            height: 12px;
            margin-left: 4px;
        }
        QMenu::indicator:checked {
            background-color: #4772B3;
            border: 1px solid #5680C2;
            border-radius: 2px;
        }
        QMenu::indicator:unchecked {
            background-color: #181818;
            border: 1px solid #444444;
            border-radius: 2px;
        }
    )");

    // ---- 0. App Logo (Static Monochromatic Icon, Left of File) ----
    auto* logoLabel = new QLabel(appMenuBar);
    logoLabel->setPixmap(IconHelper::getPixmap("logo", QColor(212, 212, 216), 18));
    logoLabel->setStyleSheet("padding: 2px 4px 2px 8px; background: transparent; border: none;");
    logoLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    appMenuBar->setCornerWidget(logoLabel, Qt::TopLeftCorner);

    // ---- 1. File Menu ----
    fileMenu = appMenuBar->addMenu("&File");

    actionExit = fileMenu->addAction("Exit");
    actionExit->setShortcut(QKeySequence("Ctrl+Q"));
    connect(actionExit, &QAction::triggered, this, &QMainWindow::close);

    // ---- 2. View Menu ----
    viewMenu = appMenuBar->addMenu("&View");

    // Online / Offline radio group
    auto* mapModeGroup = new QActionGroup(this);
    mapModeGroup->setExclusive(true);

    actionOnline = viewMenu->addAction(IconHelper::map(QColor(138, 180, 248), 16), "Online Map (OpenStreetMap - Full India)");
    actionOnline->setCheckable(true);
    actionOnline->setChecked(true);
    actionOnline->setActionGroup(mapModeGroup);
    connect(actionOnline, &QAction::triggered, this, &MainWindow::switchToOnline);

    actionOffline = viewMenu->addAction(IconHelper::map(QColor(253, 214, 99), 16), "Offline Map (Local MBTiles - Assam)");
    actionOffline->setCheckable(true);
    actionOffline->setChecked(false);
    actionOffline->setActionGroup(mapModeGroup);
    connect(actionOffline, &QAction::triggered, this, &MainWindow::switchToOffline);

    viewMenu->addSeparator();

    // Theme Submenu
    themeMenu = viewMenu->addMenu(IconHelper::system(QColor(167, 139, 250), 16), "Theme");
    auto* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    actionThemeSystem = themeMenu->addAction(IconHelper::system(QColor(138, 180, 248), 16), "System Default");
    actionThemeSystem->setCheckable(true);
    actionThemeSystem->setChecked(true);
    actionThemeSystem->setActionGroup(themeGroup);
    connect(actionThemeSystem, &QAction::triggered, this, [this]() {
        applyTheme(AppTheme::SystemDefault);
    });

    actionThemeDark = themeMenu->addAction(IconHelper::moon(QColor(167, 139, 250), 16), "Dark Theme");
    actionThemeDark->setCheckable(true);
    actionThemeDark->setActionGroup(themeGroup);
    connect(actionThemeDark, &QAction::triggered, this, [this]() {
        applyTheme(AppTheme::Dark);
    });

    actionThemeLight = themeMenu->addAction(IconHelper::sun(QColor(253, 214, 99), 16), "Light Theme");
    actionThemeLight->setCheckable(true);
    actionThemeLight->setActionGroup(themeGroup);
    connect(actionThemeLight, &QAction::triggered, this, [this]() {
        applyTheme(AppTheme::Light);
    });

    // Online Map Style Submenu
    onlineStylesMenu = viewMenu->addMenu(IconHelper::map(QColor(138, 180, 248), 16), "Tile Style Presets");
    auto* styleGroup = new QActionGroup(this);
    styleGroup->setExclusive(true);

    actionOsmStandard = onlineStylesMenu->addAction(IconHelper::map(QColor(138, 180, 248), 16), "OpenStreetMap Standard (Vibrant Color)");
    actionOsmStandard->setCheckable(true);
    actionOsmStandard->setChecked(true);
    actionOsmStandard->setActionGroup(styleGroup);
    connect(actionOsmStandard, &QAction::triggered, this, [this]() {
        onlineMap->setTileProvider(OnlineTileProvider::OpenStreetMap_Standard);
        switchToOnline();
    });

    actionOsmVoyager = onlineStylesMenu->addAction(IconHelper::map(QColor(138, 180, 248), 16), "Carto Voyager (Rich Hydro & Terrain)");
    actionOsmVoyager->setCheckable(true);
    actionOsmVoyager->setActionGroup(styleGroup);
    connect(actionOsmVoyager, &QAction::triggered, this, [this]() {
        onlineMap->setTileProvider(OnlineTileProvider::OpenStreetMap_Voyager);
        switchToOnline();
    });

    actionOsmDe = onlineStylesMenu->addAction(IconHelper::map(QColor(138, 180, 248), 16), "OpenStreetMap (Fast Mirror)");
    actionOsmDe->setCheckable(true);
    actionOsmDe->setActionGroup(styleGroup);
    connect(actionOsmDe, &QAction::triggered, this, [this]() {
        onlineMap->setTileProvider(OnlineTileProvider::OpenStreetMap_DE);
        switchToOnline();
    });

    actionOsmDark = onlineStylesMenu->addAction(IconHelper::fog(QColor(138, 180, 248), 16), "Carto Dark (Night Navigation)");
    actionOsmDark->setCheckable(true);
    actionOsmDark->setActionGroup(styleGroup);
    connect(actionOsmDark, &QAction::triggered, this, [this]() {
        onlineMap->setTileProvider(OnlineTileProvider::OpenStreetMap_Dark);
        switchToOnline();
    });

    viewMenu->addSeparator();

    actionToggleSidebar = viewMenu->addAction(IconHelper::graph(QColor(138, 180, 248), 16), "Toggle Properties Panel");
    actionToggleSidebar->setShortcut(QKeySequence("Ctrl+B"));
    connect(actionToggleSidebar, &QAction::triggered, this, &MainWindow::togglePropertiesPanel);

    actionToggleTimeline = viewMenu->addAction(IconHelper::ruler(QColor(138, 180, 248), 16), "Toggle Timeline");
    actionToggleTimeline->setShortcut(QKeySequence("Ctrl+T"));
    connect(actionToggleTimeline, &QAction::triggered, this, &MainWindow::toggleTimeline);

    viewMenu->addSeparator();

    actionFullscreen = viewMenu->addAction("Toggle Fullscreen");
    actionFullscreen->setShortcut(QKeySequence("F11"));
    connect(actionFullscreen, &QAction::triggered, this, [this]() {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    });

    // ---- 3. Settings Menu ----
    settingsMenu = appMenuBar->addMenu("&Settings");

    actionOpenSettings = settingsMenu->addAction(IconHelper::radar(QColor(138, 180, 248), 16), "Configure Settings...");
    actionOpenSettings->setShortcut(QKeySequence("Ctrl+,"));
    connect(actionOpenSettings, &QAction::triggered, this, &MainWindow::openSettingsDialog);

    settingsMenu->addSeparator();

    // Quick Zoom Sensitivity Presets
    auto* sensSubMenu = settingsMenu->addMenu(IconHelper::zoomIn(QColor(138, 180, 248), 16), "Zoom Sensitivity");
    auto* sensGroup = new QActionGroup(this);
    sensGroup->setExclusive(true);

    actionSensLow = sensSubMenu->addAction("Smooth Touchpad (30%)");
    actionSensLow->setCheckable(true);
    actionSensLow->setActionGroup(sensGroup);
    connect(actionSensLow, &QAction::triggered, this, [this]() {
        AppSettings s = settingsDialog->getSettings();
        s.zoomSensitivity = 0.3;
        settingsDialog->saveSettings();
        applyAppSettings(s);
    });

    actionSensMed = sensSubMenu->addAction("Balanced (50% - Default)");
    actionSensMed->setCheckable(true);
    actionSensMed->setChecked(true);
    actionSensMed->setActionGroup(sensGroup);
    connect(actionSensMed, &QAction::triggered, this, [this]() {
        AppSettings s = settingsDialog->getSettings();
        s.zoomSensitivity = 0.5;
        settingsDialog->saveSettings();
        applyAppSettings(s);
    });

    actionSensHigh = sensSubMenu->addAction("Standard Mouse (100%)");
    actionSensHigh->setCheckable(true);
    actionSensHigh->setActionGroup(sensGroup);
    connect(actionSensHigh, &QAction::triggered, this, [this]() {
        AppSettings s = settingsDialog->getSettings();
        s.zoomSensitivity = 1.0;
        settingsDialog->saveSettings();
        applyAppSettings(s);
    });

    // ---- 4. Vertical Separator at Right of Settings ----
    auto* sepWidget = new QWidget(appMenuBar);
    sepWidget->setFixedSize(1, 16);
    sepWidget->setStyleSheet("background-color: #383838; margin: 4px 6px;");
    auto* sepAction = new QWidgetAction(appMenuBar);
    sepAction->setDefaultWidget(sepWidget);
    appMenuBar->addAction(sepAction);

    // ---- 5. Workspace Switchers (Simulation / Analytics) ----
    auto* workspaceGroup = new QActionGroup(this);
    workspaceGroup->setExclusive(true);

    actionWorkspaceSim = appMenuBar->addAction(IconHelper::map(QColor(138, 180, 248), 16), "Simulation");
    actionWorkspaceSim->setCheckable(true);
    actionWorkspaceSim->setChecked(true);
    actionWorkspaceSim->setActionGroup(workspaceGroup);
    connect(actionWorkspaceSim, &QAction::triggered, this, &MainWindow::showSimulationScreen);

    actionWorkspaceAnalytics = appMenuBar->addAction(IconHelper::graph(QColor(167, 139, 250), 16), "Analytics");
    actionWorkspaceAnalytics->setCheckable(true);
    actionWorkspaceAnalytics->setChecked(false);
    actionWorkspaceAnalytics->setActionGroup(workspaceGroup);
    connect(actionWorkspaceAnalytics, &QAction::triggered, this, &MainWindow::showAnalyticsScreen);
}

void MainWindow::setupUi() {
    // 0. Horizontal QSplitter for Center vs Right Properties Panel
    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setStyleSheet(R"(
        QSplitter::handle:horizontal {
            background-color: #1A1A1A;
            width: 5px;
            border-left: 1px solid #141414;
            border-right: 1px solid #141414;
        }
        QSplitter::handle:horizontal:hover {
            background-color: #4772B3;
        }
        QSplitter::handle:vertical {
            background-color: #1A1A1A;
            height: 5px;
            border-top: 1px solid #141414;
            border-bottom: 1px solid #141414;
        }
        QSplitter::handle:vertical:hover {
            background-color: #4772B3;
        }
    )");

    // 1. Vertical QSplitter for Map (Top) vs Resizable Timeline (Bottom)
    vSplitter = new QSplitter(Qt::Vertical, mainSplitter);

    // Map Container Widget
    mapContainer = new QWidget(vSplitter);
    mapContainer->installEventFilter(this);

    auto* containerLayout = new QVBoxLayout(mapContainer);
    containerLayout->setContentsMargins(0, 0, 0, 0);

    // Stacked widget for switching between online and offline maps
    mapStack = new QStackedWidget(mapContainer);

    // Online Map (OpenStreetMap tiles)
    onlineMap = new OnlineTileWidget(mapContainer);
    onlineMap->setTileProvider(OnlineTileProvider::OpenStreetMap_Standard);

    // Offline Map (custom QPainter renderer from PBF data)
    mapWidget = new MapWidget(mapContainer);
    mapWidget->getRenderer().setStyle(MapRenderer::MapStyle(MapRenderer::ThemePreset::GOOGLE_DARK));

    mapStack->addWidget(onlineMap);   // index 0 = Online
    mapStack->addWidget(mapWidget);   // index 1 = Offline
    containerLayout->addWidget(mapStack);

    // Timeline Widget (Fully resizable via vSplitter)
    timelineWidget = new TimelineWidget(vSplitter);

    vSplitter->addWidget(mapContainer);
    vSplitter->addWidget(timelineWidget);
    vSplitter->setCollapsible(0, false);
    vSplitter->setCollapsible(1, true);
    vSplitter->setSizes({ 640, 140 });

    // Right Properties Panel
    propertiesPanel = new PropertiesPanel(mainSplitter);

    mainSplitter->addWidget(vSplitter);
    mainSplitter->addWidget(propertiesPanel);
    mainSplitter->setCollapsible(0, false);
    mainSplitter->setCollapsible(1, true);
    mainSplitter->setSizes({ 1060, 300 });

    // Multi-Screen Root Stack (Index 0: Simulation Workspace, Index 1: Analytics Screen)
    analyticsScreen = new QWidget(this);
    analyticsScreen->setObjectName("analyticsScreen");
    analyticsScreen->setStyleSheet("QWidget#analyticsScreen { background-color: #1A1A1A; }");

    rootStack = new QStackedWidget(this);
    rootStack->addWidget(mainSplitter);
    rootStack->addWidget(analyticsScreen);

    setCentralWidget(rootStack);

    // 2. Floating Search Bar (Top-Left)
    searchBar = new SearchBar(mapContainer);

    // 3. Floating Place Inspector Card (Left)
    placeCard = new PlaceCard(mapContainer);
    placeCard->hide();

    // 4. Floating Navigation Controls (Bottom-Right)
    navControls = new NavigationControls(mapContainer);

    // 5. Dynamic Scale Bar (Bottom-Right)
    scaleBar = new ScaleBar(mapContainer);

    // 6. Floating Layer Panel (Bottom-Left)
    layerPanel = new LayerPanel(mapContainer);
    layerPanel->hide();

    btnToggleLayers = new QPushButton("Layers", mapContainer);
    btnToggleLayers->setIcon(IconHelper::map(QColor(200, 200, 205), 16));
    btnToggleLayers->setStyleSheet(R"(
        QPushButton {
            background-color: #242424;
            color: #CCCCCC;
            border: 1px solid #383838;
            border-radius: 6px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: bold;
            padding: 5px 12px;
        }
        QPushButton:hover {
            background-color: #303030;
            color: #FFFFFF;
            border-color: #4772B3;
        }
        QPushButton:checked {
            background-color: #4772B3;
            color: #FFFFFF;
            border-color: #5680C2;
        }
    )");
    btnToggleLayers->setCheckable(true);

    auto* lShadow = new QGraphicsDropShadowEffect(mapContainer);
    lShadow->setBlurRadius(12);
    lShadow->setColor(QColor(0, 0, 0, 90));
    lShadow->setOffset(0, 2);
    btnToggleLayers->setGraphicsEffect(lShadow);

    // 7. MiniMap Overview (Bottom-Left)
    miniMap = new MiniMap(mapContainer);
    miniMap->hide();

    btnToggleMiniMap = new QPushButton("Minimap", mapContainer);
    btnToggleMiniMap->setIcon(IconHelper::radar(QColor(200, 200, 205), 16));
    btnToggleMiniMap->setStyleSheet(btnToggleLayers->styleSheet());
    btnToggleMiniMap->setCheckable(true);

    auto* mShadow = new QGraphicsDropShadowEffect(mapContainer);
    mShadow->setBlurRadius(12);
    mShadow->setColor(QColor(0, 0, 0, 90));
    mShadow->setOffset(0, 2);
    btnToggleMiniMap->setGraphicsEffect(mShadow);

    // 8. Status HUD (Bottom Status Bar)
    statusHud = new QWidget(mapContainer);
    statusHud->setStyleSheet(R"(
        QWidget {
            background-color: rgba(28, 28, 28, 0.95);
            border: 1px solid #333333;
            border-radius: 4px;
        }
        QLabel {
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            color: #CCCCCC;
        }
    )");

    auto* hudLayout = new QHBoxLayout(statusHud);
    hudLayout->setContentsMargins(8, 2, 8, 2);
    hudLayout->setSpacing(12);

    lblCoordinates = new QLabel("22.0000° N, 79.0000° E", statusHud);
    lblZoomLevel = new QLabel("Zoom: 5", statusHud);
    lblFeatureCount = new QLabel("Loading...", statusHud);
    lblFps = new QLabel("60 FPS", statusHud);
    lblFps->setStyleSheet("font-weight: bold; color: #81C995;");
    lblMapMode = new QLabel("Online", statusHud);
    lblMapMode->setStyleSheet("font-weight: bold; color: #8AB4F8;");
    lblTheme = new QLabel("System", statusHud);
    lblTheme->setStyleSheet("font-weight: bold; color: #A78BFA;");

    hudLayout->addWidget(lblMapMode);
    hudLayout->addWidget(lblTheme);
    hudLayout->addWidget(lblCoordinates);
    hudLayout->addWidget(lblZoomLevel);
    hudLayout->addWidget(lblFeatureCount);
    hudLayout->addWidget(lblFps);

    // 9. Loading Overlay
    loadingOverlay = new LoadingOverlay(mapContainer);

    // Wire up Signals & Slots
    connect(searchBar, &SearchBar::searchResultSelected, this, &MainWindow::onSearchResultSelected);

    connect(mapWidget, &MapWidget::featureSelected, this, &MainWindow::onFeatureSelected);
    connect(mapWidget, &MapWidget::viewportChanged, this, &MainWindow::onViewportChanged);
    connect(mapWidget, &MapWidget::cursorGeoMoved, this, &MainWindow::onCursorGeoMoved);
    connect(mapWidget, &MapWidget::fpsChanged, this, &MainWindow::onFpsChanged);

    // Online map viewport updates
    connect(onlineMap, &OnlineTileWidget::viewportChanged, this, [this](double lat, double lon, int zoom) {
        lblCoordinates->setText(QString("%1° N, %2° E")
            .arg(lat, 0, 'f', 4)
            .arg(lon, 0, 'f', 4));
        lblZoomLevel->setText(QString("Zoom: %1").arg(zoom));

        if (currentMapMode == MapMode::Online) {
            miniMap->setOnlineViewport(lat, lon, zoom, mapContainer->width(), mapContainer->height());
        }
    });

    // Navigation Controls
    connect(navControls, &NavigationControls::zoomInRequested, this, [this]() {
        if (currentMapMode == MapMode::Online) {
            onlineMap->zoomIn();
        } else {
            mapWidget->zoomIn();
        }
    });
    connect(navControls, &NavigationControls::zoomOutRequested, this, [this]() {
        if (currentMapMode == MapMode::Online) {
            onlineMap->zoomOut();
        } else {
            mapWidget->zoomOut();
        }
    });
    connect(navControls, &NavigationControls::fitExtentRequested, this, [this]() {
        if (currentMapMode == MapMode::Online) {
            onlineMap->fitIndia();
        } else {
            mapWidget->fitAssam();
        }
    });
    connect(navControls, &NavigationControls::resetNorthRequested, this, [this]() {
        if (currentMapMode == MapMode::Online) {
            onlineMap->fitIndia();
        } else {
            mapWidget->fitAssam();
        }
    });
    connect(navControls, &NavigationControls::measureToggled, mapWidget, &MapWidget::setMeasureMode);

    connect(placeCard, &PlaceCard::zoomInRequested, this, [this](MapCore::Point2D pos) {
        if (currentMapMode == MapMode::Online) {
            MapCore::GeoCoord geo = MapCore::Projection::mercatorToGeo(pos);
            onlineMap->setCenter(geo.lat, geo.lon);
            onlineMap->zoomIn();
        } else {
            mapWidget->flyTo(pos, mapWidget->getZoom() + 2.0f);
        }
    });
    connect(placeCard, &PlaceCard::measureFromRequested, this, [this](MapCore::Point2D pos) {
        mapWidget->setMeasureMode(true);
        navControls->setMeasureActive(true);
    });

    connect(layerPanel, &LayerPanel::themeChanged, this, [this](MapRenderer::ThemePreset theme) {
        mapWidget->getRenderer().getStyle().setTheme(theme);
        mapWidget->update();
    });
    connect(layerPanel, &LayerPanel::optionsChanged, this, [this](const MapRenderer::RenderOptions& opt) {
        mapWidget->getRenderer().setOptions(opt);
        mapWidget->update();
    });

    connect(btnToggleLayers, &QPushButton::toggled, this, [this](bool checked) {
        layerPanel->setVisible(checked);
        updateFloatingPositions();
    });

    connect(btnToggleMiniMap, &QPushButton::toggled, this, [this](bool checked) {
        miniMap->setVisible(checked);
        updateFloatingPositions();
    });

    // MiniMap Click-to-Center for Online Mode (India)
    connect(miniMap, &MiniMap::onlineCenterRequested, this, [this](double lat, double lon) {
        if (currentMapMode == MapMode::Online) {
            onlineMap->setCenter(lat, lon);
        }
    });

    // MiniMap Click-to-Center for Offline Mode (Assam)
    connect(miniMap, &MiniMap::centerRequested, this, [this](MapCore::Point2D pos) {
        if (currentMapMode == MapMode::Offline) {
            mapWidget->flyTo(pos, mapWidget->getZoom());
        }
    });

    // Timeline changes
    connect(timelineWidget, &TimelineWidget::frameChanged, this, [this](int frame, const QString& timeCode) {
        (void)frame;
        (void)timeCode;
    });

    updateFloatingPositions();
}

void MainWindow::switchToOnline() {
    currentMapMode = MapMode::Online;
    mapStack->setCurrentIndex(0);
    actionOnline->setChecked(true);

    lblMapMode->setText("Online");
    lblMapMode->setStyleSheet("font-weight: bold; color: #8AB4F8;");

    miniMap->setMode(MiniMapMode::Online_India);
    miniMap->setOnlineViewport(onlineMap->getCenterLat(), onlineMap->getCenterLon(), onlineMap->getZoom(), mapContainer->width(), mapContainer->height());

    btnToggleLayers->setVisible(false);
    layerPanel->hide();

    updateFloatingPositions();
}

void MainWindow::switchToOffline() {
    currentMapMode = MapMode::Offline;
    mapStack->setCurrentIndex(1);
    actionOffline->setChecked(true);

    lblMapMode->setText("Offline");
    lblMapMode->setStyleSheet("font-weight: bold; color: #FDD663;");

    miniMap->setMode(MiniMapMode::Offline_Assam);
    if (mapWidget->getSpatialIndex()) {
        miniMap->setSpatialIndex(mapWidget->getSpatialIndex());
    }

    btnToggleLayers->setVisible(true);

    updateFloatingPositions();
}

void MainWindow::startAsyncLoad() {
    loadingOverlay->show();

    (void)QtConcurrent::run([this]() {
        bool loaded = false;

        if (MapCore::MapDataCache::isCacheValid(cacheFilePath, dataPbfPath)) {
            QMetaObject::invokeMethod(this, [this]() {
                loadingOverlay->setProgress(0.3f, "Loading fast binary cache...");
            });

            loaded = MapCore::MapDataCache::loadCache(cacheFilePath, spatialIndex);
        }

        if (!loaded) {
            QMetaObject::invokeMethod(this, [this]() {
                loadingOverlay->setProgress(0.05f, "Parsing OSM PBF data (Assam)...");
            });

            loaded = MapCore::OsmPbfLoader::loadPbfFile(dataPbfPath, spatialIndex,
                [this](float progress, const std::string& msg) {
                    QMetaObject::invokeMethod(this, [this, progress, msg]() {
                        loadingOverlay->setProgress(progress, QString::fromStdString(msg));
                    });
                });

            if (loaded) {
                QMetaObject::invokeMethod(this, [this]() {
                    loadingOverlay->setProgress(0.95f, "Writing instant binary cache...");
                });
                MapCore::MapDataCache::saveCache(cacheFilePath, spatialIndex);
            }
        }

        QMetaObject::invokeMethod(this, &MainWindow::onMapLoaded);
    });
}

void MainWindow::onMapLoaded() {
    mapWidget->setSpatialIndex(&spatialIndex);
    searchBar->setSearchIndex(spatialIndex.searchItems);
    miniMap->setSpatialIndex(&spatialIndex);

    size_t totalFeatures = spatialIndex.polygons.size() + spatialIndex.polylines.size() + spatialIndex.points.size();
    lblFeatureCount->setText(QString("%1 features loaded").arg(totalFeatures));

    loadingOverlay->hide();
    updateFloatingPositions();
}

void MainWindow::onSearchResultSelected(MapCore::Point2D pos, float targetZoom,
                                       QString name, QString detail, MapCore::FeatureCategory category) {
    if (currentMapMode == MapMode::Online) {
        MapCore::GeoCoord geo = MapCore::Projection::mercatorToGeo(pos);
        onlineMap->setCenter(geo.lat, geo.lon);
        onlineMap->setZoom(static_cast<int>(std::round(targetZoom)));
    } else {
        mapWidget->flyTo(pos, targetZoom);
        mapWidget->setSelectedPosition(pos);
    }

    MapCore::FeatureInfo info;
    info.found = true;
    info.category = category;
    info.name = name.toStdString();
    info.detail = detail.toStdString();
    info.mercatorPos = pos;
    info.geoCoord = MapCore::Projection::mercatorToGeo(pos);

    placeCard->setFeature(info);
    updateFloatingPositions();
}

void MainWindow::onFeatureSelected(MapCore::FeatureInfo info) {
    placeCard->setFeature(info);
    updateFloatingPositions();
}

void MainWindow::onViewportChanged(MapCore::BoundingBox viewBbox, float zoomLevel, MapCore::GeoCoord centerGeo) {
    scaleBar->updateScale(zoomLevel, centerGeo.lat, mapContainer->width());
    lblZoomLevel->setText(QString("Zoom: %1").arg(zoomLevel, 0, 'f', 1));

    if (currentMapMode == MapMode::Offline) {
        miniMap->setViewport(viewBbox);
    }
}

void MainWindow::onCursorGeoMoved(MapCore::GeoCoord geo, QString hoverText) {
    if (!hoverText.isEmpty()) {
        lblCoordinates->setText(QString("%1, %2 (%3)")
            .arg(geo.lat, 0, 'f', 4)
            .arg(geo.lon, 0, 'f', 4)
            .arg(hoverText));
    } else {
        lblCoordinates->setText(QString("%1° N, %2° E")
            .arg(geo.lat, 0, 'f', 4)
            .arg(geo.lon, 0, 'f', 4));
    }
}

void MainWindow::onFpsChanged(float fps) {
    lblFps->setText(QString("%1 FPS").arg(qRound(fps)));
    if (fps >= 50.0f) {
        lblFps->setStyleSheet("font-weight: bold; color: #81C995;");
    } else if (fps >= 30.0f) {
        lblFps->setStyleSheet("font-weight: bold; color: #FDD663;");
    } else {
        lblFps->setStyleSheet("font-weight: bold; color: #F28B82;");
    }
}

void MainWindow::resizeEvent(QResizeEvent* /*event*/) {
    updateFloatingPositions();
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == mapContainer && event->type() == QEvent::Resize) {
        updateFloatingPositions();
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateFloatingPositions() {
    if (!mapContainer) return;

    int w = mapContainer->width();
    int h = mapContainer->height();

    // 1. Search Bar at Top-Left
    searchBar->move(20, 20);

    // 2. Place Card below Search Bar
    if (placeCard->isVisible()) {
        placeCard->move(20, searchBar->y() + searchBar->height() + 10);
    }

    // 3. Navigation Controls at Bottom-Right
    int navW = navControls->sizeHint().width();
    int navH = navControls->sizeHint().height();
    navControls->setGeometry(w - navW - 20, h - navH - 45, navW, navH);

    // 4. Scale Bar at Bottom-Right (left of nav controls)
    scaleBar->move(w - navW - 20 - scaleBar->width() - 15, h - 50);

    // 5. Status HUD at Bottom-Center
    statusHud->adjustSize();
    statusHud->move((w - statusHud->width()) / 2, h - statusHud->height() - 8);

    // 6. Layer Panel and Minimap at Bottom-Left
    int leftBottomY = h - 45;

    btnToggleLayers->adjustSize();
    btnToggleLayers->move(20, leftBottomY);

    btnToggleMiniMap->adjustSize();
    btnToggleMiniMap->move(20 + (btnToggleLayers->isVisible() ? btnToggleLayers->width() + 8 : 0), leftBottomY);

    if (layerPanel->isVisible()) {
        layerPanel->move(20, leftBottomY - layerPanel->sizeHint().height() - 10);
    }

    if (miniMap->isVisible()) {
        int miniY = (layerPanel->isVisible() ? layerPanel->y() - miniMap->height() - 10 : leftBottomY - miniMap->height() - 10);
        miniMap->move(20, miniY);
    }

    // 7. Loading Overlay covering entire map area
    loadingOverlay->setGeometry(0, 0, w, h);
}

void MainWindow::showSimulationScreen() {
    if (rootStack) {
        rootStack->setCurrentIndex(0);
    }
    if (actionWorkspaceSim) {
        actionWorkspaceSim->setChecked(true);
    }
    updateFloatingPositions();
}

void MainWindow::showAnalyticsScreen() {
    if (rootStack) {
        rootStack->setCurrentIndex(1);
    }
    if (actionWorkspaceAnalytics) {
        actionWorkspaceAnalytics->setChecked(true);
    }
}

} // namespace MapUI
