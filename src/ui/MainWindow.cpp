#include "MainWindow.h"
#include "../core/OsmPbfLoader.h"
#include "../core/MapDataCache.h"
#include <QtConcurrent/QtConcurrent>
#include <QGraphicsDropShadowEffect>
#include <QResizeEvent>
#include <QApplication>
#include <QStyle>
#include <QProcess>
#include <iostream>

namespace MapUI {

LoadingOverlay::LoadingOverlay(QWidget* parent) : QWidget(parent) {
    setStyleSheet(R"(
        QWidget#loadingCard {
            background-color: #202124;
            border: 1px solid #3C4043;
            border-radius: 16px;
        }
        QProgressBar {
            border: none;
            border-radius: 4px;
            background-color: #303134;
            height: 8px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #8AB4F8;
            border-radius: 4px;
        }
    )");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setAlignment(Qt::AlignCenter);

    auto* card = new QWidget(this);
    card->setObjectName("loadingCard");
    card->setFixedSize(360, 200);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setColor(QColor(0, 0, 0, 120));
    shadow->setOffset(0, 8);
    card->setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(24, 20, 24, 20);
    layout->setSpacing(12);

    lblLogo = new QLabel("🗺️ Assam & India Maps", card);
    lblLogo->setAlignment(Qt::AlignCenter);
    lblLogo->setStyleSheet("font-family: 'Segoe UI', Arial, sans-serif; font-size: 20px; font-weight: bold; color: #8AB4F8;");

    auto* lblSub = new QLabel("High Performance OpenStreetMap Engine", card);
    lblSub->setAlignment(Qt::AlignCenter);
    lblSub->setStyleSheet("font-family: 'Segoe UI', Arial, sans-serif; font-size: 11px; color: #9AA0A6;");

    progressBar = new QProgressBar(card);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(false);

    lblStatus = new QLabel("Initializing engine...", card);
    lblStatus->setAlignment(Qt::AlignCenter);
    lblStatus->setStyleSheet("font-family: 'Segoe UI', Arial, sans-serif; font-size: 12px; color: #E8EAED;");

    layout->addWidget(lblLogo);
    layout->addWidget(lblSub);
    layout->addSpacing(8);
    layout->addWidget(progressBar);
    layout->addWidget(lblStatus);

    rootLayout->addWidget(card);
}

void LoadingOverlay::setProgress(float progress, const QString& status) {
    progressBar->setValue(static_cast<int>(progress * 100));
    lblStatus->setText(status);
}

bool MainWindow::isSystemDarkTheme() {
    // 1. Query GNOME / FreeDesktop portal color scheme setting on Linux
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

    // 2. Check QPalette background luminance
    QColor winColor = QGuiApplication::palette().color(QPalette::Window);
    if (winColor.isValid()) {
        return winColor.value() < 128;
    }

    // Default to dark theme for modern mapping application
    return true;
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("Assam & India Maps");
    resize(1280, 800);
    setMinimumSize(800, 600);

    setupUi();
    setupMenuBar();

    // Create Settings dialog
    settingsDialog = new SettingsDialog(this);
    connect(settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::applyAppSettings);

    // Apply saved or default settings
    applyAppSettings(settingsDialog->getSettings());

    // Start async offline data load in background
    startAsyncLoad();
}

void MainWindow::applyTheme(AppTheme theme) {
    currentTheme = theme;
    bool isDark = false;

    if (theme == AppTheme::SystemDefault) {
        isDark = isSystemDarkTheme();
        lblTheme->setText(isDark ? "🖥️ System (Dark)" : "🖥️ System (Light)");
    } else if (theme == AppTheme::Dark) {
        isDark = true;
        lblTheme->setText("🌙 Dark");
    } else {
        isDark = false;
        lblTheme->setText("☀️ Light");
    }

    // Update Online Map mode & provider
    onlineMap->setDarkMode(isDark);

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

    std::cout << "[INFO] Applied app settings (Zoom sensitivity: " << static_cast<int>(settings.zoomSensitivity * 100)
              << "%, Mode: " << (settings.startupMode == 0 ? "Online" : "Offline") << ")" << std::endl;
}

void MainWindow::openSettingsDialog() {
    if (!settingsDialog) {
        settingsDialog = new SettingsDialog(this);
        connect(settingsDialog, &SettingsDialog::settingsApplied, this, &MainWindow::applyAppSettings);
    }
    settingsDialog->loadSettings();
    settingsDialog->exec();
}

void MainWindow::setupMenuBar() {
    // Style the menu bar with high contrast, bright white text
    appMenuBar = menuBar();
    appMenuBar->setStyleSheet(R"(
        QMenuBar {
            background-color: #18181B;
            color: #FFFFFF;
            border-bottom: 1px solid #3F3F46;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
            font-weight: bold;
            padding: 3px 6px;
        }
        QMenuBar::item {
            background: transparent;
            padding: 6px 14px;
            border-radius: 4px;
            color: #FFFFFF;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
            font-weight: bold;
        }
        QMenuBar::item:selected {
            background-color: #27272A;
            color: #8AB4F8;
        }
        QMenuBar::item:pressed {
            background-color: #3F3F46;
            color: #FFFFFF;
        }
        QMenu {
            background-color: #18181B;
            color: #FFFFFF;
            border: 1px solid #3F3F46;
            border-radius: 6px;
            padding: 4px 0px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
        }
        QMenu::item {
            padding: 8px 24px 8px 14px;
            color: #FFFFFF;
            font-size: 13px;
            font-weight: 500;
        }
        QMenu::item:selected {
            background-color: #27272A;
            color: #8AB4F8;
        }
        QMenu::separator {
            height: 1px;
            background-color: #3F3F46;
            margin: 4px 8px;
        }
        QMenu::indicator {
            width: 14px;
            height: 14px;
            margin-left: 6px;
        }
        QMenu::indicator:checked {
            background-color: #8AB4F8;
            border: 1px solid #8AB4F8;
            border-radius: 3px;
        }
        QMenu::indicator:unchecked {
            background-color: transparent;
            border: 1px solid #71717A;
            border-radius: 3px;
        }
    )");

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

    actionOnline = viewMenu->addAction("🌐  Online Map (OpenStreetMap - Full India)");
    actionOnline->setCheckable(true);
    actionOnline->setChecked(true);  // Default to online
    actionOnline->setActionGroup(mapModeGroup);
    connect(actionOnline, &QAction::triggered, this, &MainWindow::switchToOnline);

    actionOffline = viewMenu->addAction("💾  Offline Map (Local MBTiles - Assam)");
    actionOffline->setCheckable(true);
    actionOffline->setChecked(false);
    actionOffline->setActionGroup(mapModeGroup);
    connect(actionOffline, &QAction::triggered, this, &MainWindow::switchToOffline);

    viewMenu->addSeparator();

    // Theme Submenu (System Default, Dark, Light)
    themeMenu = viewMenu->addMenu("🎨  Theme");
    auto* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    actionThemeSystem = themeMenu->addAction("🖥️  System Default");
    actionThemeSystem->setCheckable(true);
    actionThemeSystem->setChecked(true); // Default to system default
    actionThemeSystem->setActionGroup(themeGroup);
    connect(actionThemeSystem, &QAction::triggered, this, [this]() {
        applyTheme(AppTheme::SystemDefault);
    });

    actionThemeDark = themeMenu->addAction("🌙  Dark Theme");
    actionThemeDark->setCheckable(true);
    actionThemeDark->setActionGroup(themeGroup);
    connect(actionThemeDark, &QAction::triggered, this, [this]() {
        applyTheme(AppTheme::Dark);
    });

    actionThemeLight = themeMenu->addAction("☀️  Light Theme");
    actionThemeLight->setCheckable(true);
    actionThemeLight->setActionGroup(themeGroup);
    connect(actionThemeLight, &QAction::triggered, this, [this]() {
        applyTheme(AppTheme::Light);
    });

    // Online Map Style Submenu
    onlineStylesMenu = viewMenu->addMenu("🗺️  Tile Style Presets");
    auto* styleGroup = new QActionGroup(this);
    styleGroup->setExclusive(true);

    actionOsmStandard = onlineStylesMenu->addAction("🗺️  OpenStreetMap Standard (Light)");
    actionOsmStandard->setCheckable(true);
    actionOsmStandard->setActionGroup(styleGroup);
    connect(actionOsmStandard, &QAction::triggered, this, [this]() {
        onlineMap->setTileProvider(OnlineTileProvider::OpenStreetMap_Standard);
        switchToOnline();
    });

    actionOsmDark = onlineStylesMenu->addAction("🌙  OpenStreetMap Dark Mode");
    actionOsmDark->setCheckable(true);
    actionOsmDark->setActionGroup(styleGroup);
    connect(actionOsmDark, &QAction::triggered, this, [this]() {
        onlineMap->setTileProvider(OnlineTileProvider::OpenStreetMap_Dark);
        switchToOnline();
    });

    actionOsmVoyager = onlineStylesMenu->addAction("🌍  OpenStreetMap (Voyager Color)");
    actionOsmVoyager->setCheckable(true);
    actionOsmVoyager->setActionGroup(styleGroup);
    connect(actionOsmVoyager, &QAction::triggered, this, [this]() {
        onlineMap->setTileProvider(OnlineTileProvider::OpenStreetMap_Voyager);
        switchToOnline();
    });

    actionOsmDe = onlineStylesMenu->addAction("🇩🇪  OpenStreetMap (Fast Mirror)");
    actionOsmDe->setCheckable(true);
    actionOsmDe->setActionGroup(styleGroup);
    connect(actionOsmDe, &QAction::triggered, this, [this]() {
        onlineMap->setTileProvider(OnlineTileProvider::OpenStreetMap_DE);
        switchToOnline();
    });

    viewMenu->addSeparator();

    actionFullscreen = viewMenu->addAction("⛶  Toggle Fullscreen");
    actionFullscreen->setShortcut(QKeySequence("F11"));
    connect(actionFullscreen, &QAction::triggered, this, [this]() {
        if (isFullScreen()) {
            showNormal();
        } else {
            showFullScreen();
        }
    });

    // ---- 3. Settings Menu (Adjacent to View) ----
    settingsMenu = appMenuBar->addMenu("&Settings");

    actionOpenSettings = settingsMenu->addAction("⚙️  Configure Settings...");
    actionOpenSettings->setShortcut(QKeySequence("Ctrl+,"));
    connect(actionOpenSettings, &QAction::triggered, this, &MainWindow::openSettingsDialog);

    settingsMenu->addSeparator();

    // Quick Zoom Sensitivity Preset Actions
    auto* sensSubMenu = settingsMenu->addMenu("🔍  Zoom Sensitivity");
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
}

void MainWindow::setupUi() {
    // 0. Create stacked widget for switching between online and offline maps
    mapStack = new QStackedWidget(this);

    // Online Map (OpenStreetMap tiles)
    onlineMap = new OnlineTileWidget(this);

    // Offline Map (custom QPainter renderer from PBF data)
    mapWidget = new MapWidget(this);
    mapWidget->getRenderer().setStyle(MapRenderer::MapStyle(MapRenderer::ThemePreset::GOOGLE_DARK));

    mapStack->addWidget(onlineMap);   // index 0 = Online
    mapStack->addWidget(mapWidget);   // index 1 = Offline

    setCentralWidget(mapStack);

    // 2. Floating Search Bar (Top-Left)
    searchBar = new SearchBar(this);

    // 3. Floating Place Inspector Card (Left)
    placeCard = new PlaceCard(this);
    placeCard->hide();

    // 4. Floating Navigation Controls (Bottom-Right)
    navControls = new NavigationControls(this);

    // 5. Dynamic Scale Bar (Bottom-Right)
    scaleBar = new ScaleBar(this);

    // 6. Floating Layer Panel (Bottom-Left)
    layerPanel = new LayerPanel(this);
    layerPanel->hide();

    btnToggleLayers = new QPushButton("🥞 Layers", this);
    btnToggleLayers->setStyleSheet(R"(
        QPushButton {
            background-color: #202124;
            color: #FFFFFF;
            border: 1px solid #5F6368;
            border-radius: 18px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 12px;
            font-weight: bold;
            padding: 6px 14px;
        }
        QPushButton:hover {
            background-color: #303134;
            color: #8AB4F8;
            border-color: #8AB4F8;
        }
        QPushButton:checked {
            background-color: #8AB4F8;
            color: #202124;
            border-color: #8AB4F8;
        }
    )");
    btnToggleLayers->setCheckable(true);

    auto* lShadow = new QGraphicsDropShadowEffect(this);
    lShadow->setBlurRadius(14);
    lShadow->setColor(QColor(0, 0, 0, 80));
    lShadow->setOffset(0, 3);
    btnToggleLayers->setGraphicsEffect(lShadow);

    // 7. MiniMap Overview (Bottom-Left) - Defaults to Online India Mode
    miniMap = new MiniMap(this);
    miniMap->hide();

    btnToggleMiniMap = new QPushButton("🗺️ Minimap", this);
    btnToggleMiniMap->setStyleSheet(btnToggleLayers->styleSheet());
    btnToggleMiniMap->setCheckable(true);

    auto* mShadow = new QGraphicsDropShadowEffect(this);
    mShadow->setBlurRadius(14);
    mShadow->setColor(QColor(0, 0, 0, 80));
    mShadow->setOffset(0, 3);
    btnToggleMiniMap->setGraphicsEffect(mShadow);

    // 8. Status HUD (Bottom Status Bar, Dark Glass Surface)
    statusHud = new QWidget(this);
    statusHud->setStyleSheet(R"(
        QWidget {
            background-color: rgba(32, 33, 36, 0.95);
            border: 1px solid #3C4043;
            border-radius: 6px;
        }
        QLabel {
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            color: #E8EAED;
        }
    )");

    auto* hudLayout = new QHBoxLayout(statusHud);
    hudLayout->setContentsMargins(10, 3, 10, 3);
    hudLayout->setSpacing(14);

    lblCoordinates = new QLabel("22.0000° N, 79.0000° E", statusHud);
    lblZoomLevel = new QLabel("Zoom: 5", statusHud);
    lblFeatureCount = new QLabel("Loading...", statusHud);
    lblFps = new QLabel("60 FPS", statusHud);
    lblFps->setStyleSheet("font-weight: bold; color: #81C995;");
    lblMapMode = new QLabel("🌐 Online", statusHud);
    lblMapMode->setStyleSheet("font-weight: bold; color: #8AB4F8;");
    lblTheme = new QLabel("🖥️ System", statusHud);
    lblTheme->setStyleSheet("font-weight: bold; color: #A78BFA;");

    hudLayout->addWidget(lblMapMode);
    hudLayout->addWidget(lblTheme);
    hudLayout->addWidget(lblCoordinates);
    hudLayout->addWidget(lblZoomLevel);
    hudLayout->addWidget(lblFeatureCount);
    hudLayout->addWidget(lblFps);

    // 9. Loading Overlay
    loadingOverlay = new LoadingOverlay(this);

    // Wire up Signals & Slots
    connect(searchBar, &SearchBar::searchResultSelected, this, &MainWindow::onSearchResultSelected);

    connect(mapWidget, &MapWidget::featureSelected, this, &MainWindow::onFeatureSelected);
    connect(mapWidget, &MapWidget::viewportChanged, this, &MainWindow::onViewportChanged);
    connect(mapWidget, &MapWidget::cursorGeoMoved, this, &MainWindow::onCursorGeoMoved);
    connect(mapWidget, &MapWidget::fpsChanged, this, &MainWindow::onFpsChanged);

    // Online map viewport updates (also updates dynamic MiniMap)
    connect(onlineMap, &OnlineTileWidget::viewportChanged, this, [this](double lat, double lon, int zoom) {
        lblCoordinates->setText(QString("%1° N, %2° E")
            .arg(lat, 0, 'f', 4)
            .arg(lon, 0, 'f', 4));
        lblZoomLevel->setText(QString("Zoom: %1").arg(zoom));

        if (currentMapMode == MapMode::Online) {
            miniMap->setOnlineViewport(lat, lon, zoom, width(), height());
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

    updateFloatingPositions();
}

void MainWindow::switchToOnline() {
    currentMapMode = MapMode::Online;
    mapStack->setCurrentIndex(0);
    actionOnline->setChecked(true);

    lblMapMode->setText("🌐 Online");
    lblMapMode->setStyleSheet("font-weight: bold; color: #8AB4F8;");

    // Configure MiniMap for Full India Mode
    miniMap->setMode(MiniMapMode::Online_India);
    miniMap->setOnlineViewport(onlineMap->getCenterLat(), onlineMap->getCenterLon(), onlineMap->getZoom(), width(), height());

    // Hide offline-only controls
    btnToggleLayers->setVisible(false);
    layerPanel->hide();

    updateFloatingPositions();
}

void MainWindow::switchToOffline() {
    currentMapMode = MapMode::Offline;
    mapStack->setCurrentIndex(1);
    actionOffline->setChecked(true);

    lblMapMode->setText("💾 Offline");
    lblMapMode->setStyleSheet("font-weight: bold; color: #FDD663;");

    // Configure MiniMap for Assam Offline Mode
    miniMap->setMode(MiniMapMode::Offline_Assam);
    if (mapWidget->getSpatialIndex()) {
        miniMap->setSpatialIndex(mapWidget->getSpatialIndex());
    }

    // Show offline-specific controls
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
    scaleBar->updateScale(zoomLevel, centerGeo.lat, width());
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

void MainWindow::updateFloatingPositions() {
    int w = width();
    int h = height();

    // 1. Search Bar at Top-Left
    searchBar->move(20, 20 + appMenuBar->height());

    // 2. Place Card below Search Bar
    if (placeCard->isVisible()) {
        placeCard->move(20, searchBar->y() + searchBar->height() + 10);
    }

    // 3. Navigation Controls at Bottom-Right
    int navW = navControls->sizeHint().width();
    int navH = navControls->sizeHint().height();
    navControls->setGeometry(w - navW - 20, h - navH - 50, navW, navH);

    // 4. Scale Bar at Bottom-Right (left of nav controls)
    scaleBar->move(w - navW - 20 - scaleBar->width() - 15, h - 55);

    // 5. Status HUD at Bottom-Center
    statusHud->adjustSize();
    statusHud->move((w - statusHud->width()) / 2, h - statusHud->height() - 10);

    // 6. Layer Panel and Minimap at Bottom-Left
    int leftBottomY = h - 50;

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

    // 7. Loading Overlay covering entire window
    loadingOverlay->setGeometry(0, 0, w, h);
}

} // namespace MapUI
