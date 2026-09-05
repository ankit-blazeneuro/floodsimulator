#include "SeaLevelHeatMapWidget.h"
#include "IconHelper.h"
#include <QGraphicsDropShadowEffect>
#include <QResizeEvent>
#include <QButtonGroup>

namespace MapUI {

SeaLevelHeatMapWidget::SeaLevelHeatMapWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName("seaLevelHeatMapRoot");
    setStyleSheet("QWidget#seaLevelHeatMapRoot { background-color: #121214; }");

    setupUi();
    setupConnections();
    updateLegendGradient();
    updateMetrics();
    updateFloatingPositions();
}

void SeaLevelHeatMapWidget::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Full-Screen Map Area
    mapWidget = new SeaLevelTileWidget(this);
    mapWidget->installEventFilter(this);
    rootLayout->addWidget(mapWidget);

    // -------------------------------------------------------------
    // 1. Floating Sea Level Control Panel (Top-Left)
    // -------------------------------------------------------------
    controlCard = new QWidget(this);
    controlCard->setObjectName("seaLevelControlCard");
    controlCard->setFixedWidth(340);
    controlCard->setStyleSheet(R"(
        QWidget#seaLevelControlCard {
            background-color: rgba(24, 24, 27, 0.95);
            border: 1px solid #3F3F46;
            border-radius: 12px;
        }
        QLabel {
            font-family: 'Segoe UI', Arial, sans-serif;
            color: #E4E4E7;
            font-size: 11px;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #27272A;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #38BDF8, stop:1 #F43F5E);
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #FFFFFF;
            border: 2px solid #38BDF8;
            width: 16px;
            margin-top: -5px;
            margin-bottom: -5px;
            border-radius: 8px;
        }
        QComboBox {
            background-color: #18181B;
            color: #F4F4F5;
            border: 1px solid #3F3F46;
            border-radius: 6px;
            padding: 4px 8px;
            font-size: 11px;
        }
        QComboBox QAbstractItemView {
            background-color: #18181B;
            color: #F4F4F5;
            selection-background-color: #2563EB;
            border: 1px solid #3F3F46;
        }
        QCheckBox {
            color: #D4D4D8;
            font-size: 11px;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border-radius: 3px;
            border: 1px solid #52525B;
            background-color: #18181B;
        }
        QCheckBox::indicator:checked {
            background-color: #38BDF8;
            border-color: #38BDF8;
        }
    )");

    auto* cardShadow = new QGraphicsDropShadowEffect(this);
    cardShadow->setBlurRadius(20);
    cardShadow->setColor(QColor(0, 0, 0, 160));
    cardShadow->setOffset(0, 4);
    controlCard->setGraphicsEffect(cardShadow);

    auto* cardLayout = new QVBoxLayout(controlCard);
    cardLayout->setContentsMargins(14, 12, 14, 12);
    cardLayout->setSpacing(8);

    // Title Row
    auto* headerLayout = new QHBoxLayout();
    auto* iconLbl = new QLabel(controlCard);
    iconLbl->setPixmap(IconHelper::getPixmap("temperature", QColor(251, 146, 60), 20));
    auto* titleLbl = new QLabel("Sea Level & Elevation Heat Map", controlCard);
    titleLbl->setStyleSheet("font-size: 13px; font-weight: bold; color: #FFFFFF;");
    headerLayout->addWidget(iconLbl);
    headerLayout->addWidget(titleLbl);
    headerLayout->addStretch();
    cardLayout->addLayout(headerLayout);

    auto* subLbl = new QLabel("Real-Time Coastal Inundation & Topographic DEM", controlCard);
    subLbl->setStyleSheet("color: #A1A1AA; font-size: 10px; margin-top: -4px;");
    cardLayout->addWidget(subLbl);

    // Separator line
    auto* sep1 = new QWidget(controlCard);
    sep1->setFixedHeight(1);
    sep1->setStyleSheet("background-color: #333338;");
    cardLayout->addWidget(sep1);

    // Sea Level Rise Header & Value
    auto* slRow = new QHBoxLayout();
    auto* slTitle = new QLabel("Simulated Sea Level Rise:", controlCard);
    slTitle->setStyleSheet("font-weight: 600; font-size: 11px;");
    lblSeaLevelVal = new QLabel("+0.0 m MSL", controlCard);
    lblSeaLevelVal->setStyleSheet("font-size: 13px; font-weight: bold; color: #38BDF8;");
    slRow->addWidget(slTitle);
    slRow->addStretch();
    slRow->addWidget(lblSeaLevelVal);
    cardLayout->addLayout(slRow);

    // Sea Level Slider (0 to 200 = 0.0m to 20.0m)
    sldSeaLevel = new QSlider(Qt::Horizontal, controlCard);
    sldSeaLevel->setRange(0, 200);
    sldSeaLevel->setValue(0);
    cardLayout->addWidget(sldSeaLevel);

    // Risk Tag
    lblRiskTag = new QLabel("● BASELINE MEAN SEA LEVEL", controlCard);
    lblRiskTag->setStyleSheet("font-size: 10px; font-weight: bold; color: #9CA3AF;");
    cardLayout->addWidget(lblRiskTag);

    // Scenario Preset Chips
    auto* presetChipsLayout = new QGridLayout();
    presetChipsLayout->setSpacing(4);

    auto createChip = [this, presetChipsLayout](const QString& title, double meters, int row, int col) {
        auto* btn = new QPushButton(title, controlCard);
        btn->setStyleSheet(R"(
            QPushButton {
                background-color: #27272A;
                color: #D4D4D8;
                border: 1px solid #3F3F46;
                border-radius: 4px;
                font-size: 10px;
                padding: 3px 6px;
            }
            QPushButton:hover {
                background-color: #3F3F46;
                color: #FFFFFF;
                border-color: #38BDF8;
            }
        )");
        connect(btn, &QPushButton::clicked, this, [this, meters]() {
            sldSeaLevel->setValue(static_cast<int>(meters * 10.0));
        });
        presetChipsLayout->addWidget(btn, row, col);
    };

    createChip("+0.0m Baseline", 0.0, 0, 0);
    createChip("+0.8m 2050", 0.8, 0, 1);
    createChip("+1.5m 2100", 1.5, 0, 2);
    createChip("+3.0m Surge", 3.0, 1, 0);
    createChip("+5.0m Cat 5", 5.0, 1, 1);
    createChip("+10.0m Tsunami", 10.0, 1, 2);
    cardLayout->addLayout(presetChipsLayout);

    // Simulation Playback Controls
    auto* simBtnLayout = new QHBoxLayout();
    btnPlaySim = new QPushButton(" ▶ Play Tide Sim", controlCard);
    btnPlaySim->setStyleSheet(R"(
        QPushButton {
            background-color: #059669;
            color: #FFFFFF;
            border: none;
            border-radius: 5px;
            font-size: 11px;
            font-weight: 600;
            padding: 5px 10px;
        }
        QPushButton:hover {
            background-color: #10B981;
        }
    )");

    btnResetSim = new QPushButton(" ↺ Reset", controlCard);
    btnResetSim->setStyleSheet(R"(
        QPushButton {
            background-color: #27272A;
            color: #E4E4E7;
            border: 1px solid #3F3F46;
            border-radius: 5px;
            font-size: 11px;
            padding: 5px 10px;
        }
        QPushButton:hover {
            background-color: #3F3F46;
        }
    )");

    simBtnLayout->addWidget(btnPlaySim);
    simBtnLayout->addWidget(btnResetSim);
    cardLayout->addLayout(simBtnLayout);

    // Visual Settings Separator
    auto* sep2 = new QWidget(controlCard);
    sep2->setFixedHeight(1);
    sep2->setStyleSheet("background-color: #333338;");
    cardLayout->addWidget(sep2);

    // Palette Selector
    auto* palRow = new QHBoxLayout();
    auto* palLbl = new QLabel("Color Palette:", controlCard);
    cmbPalette = new QComboBox(controlCard);
    cmbPalette->addItem("🌊 Sea Level & Inundation", 0);
    cmbPalette->addItem("🌈 Turbo (Spectral DEM)", 1);
    cmbPalette->addItem("🔥 Thermal Magma", 2);
    cmbPalette->addItem("🌐 Ocean Bathymetry", 3);
    cmbPalette->addItem("🍃 Viridis Standard", 4);
    palRow->addWidget(palLbl);
    palRow->addWidget(cmbPalette);
    cardLayout->addLayout(palRow);

    // Opacity Slider
    auto* opRow = new QHBoxLayout();
    auto* opLbl = new QLabel("Heat Opacity:", controlCard);
    sldOpacity = new QSlider(Qt::Horizontal, controlCard);
    sldOpacity->setRange(10, 100);
    sldOpacity->setValue(70);
    opRow->addWidget(opLbl);
    opRow->addWidget(sldOpacity);
    cardLayout->addLayout(opRow);

    // Feature Toggles
    auto* togLayout = new QHBoxLayout();
    chkContours = new QCheckBox("Contours", controlCard);
    chkContours->setChecked(true);
    chkRipples = new QCheckBox("Surge Waves", controlCard);
    chkRipples->setChecked(true);
    chkHotspots = new QCheckBox("Hotspots", controlCard);
    chkHotspots->setChecked(true);

    togLayout->addWidget(chkContours);
    togLayout->addWidget(chkRipples);
    togLayout->addWidget(chkHotspots);
    cardLayout->addLayout(togLayout);

    // Metrics Card (Inundated Area & Population Impact)
    auto* metricsBox = new QWidget(controlCard);
    metricsBox->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; border-radius: 6px;");
    auto* mLayout = new QVBoxLayout(metricsBox);
    mLayout->setContentsMargins(10, 8, 10, 8);
    mLayout->setSpacing(4);

    lblInundatedArea = new QLabel("Inundated Land: 0 km²", metricsBox);
    lblInundatedArea->setStyleSheet("font-size: 11px; font-weight: bold; color: #F43F5E;");

    lblVulnerablePop = new QLabel("Vulnerable Pop: 0.00 M", metricsBox);
    lblVulnerablePop->setStyleSheet("font-size: 11px; font-weight: bold; color: #F59E0B;");

    mLayout->addWidget(lblInundatedArea);
    mLayout->addWidget(lblVulnerablePop);
    cardLayout->addWidget(metricsBox);

    // -------------------------------------------------------------
    // 2. Floating Location Presets Bar (Top-Right)
    // -------------------------------------------------------------
    presetsBar = new QWidget(this);
    presetsBar->setStyleSheet(R"(
        QWidget {
            background-color: rgba(24, 24, 27, 0.92);
            border: 1px solid #3F3F46;
            border-radius: 8px;
        }
        QPushButton {
            background-color: transparent;
            color: #D4D4D8;
            border: 1px solid transparent;
            border-radius: 5px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: 600;
            padding: 4px 9px;
        }
        QPushButton:hover {
            background-color: #27272A;
            color: #FFFFFF;
            border-color: #38BDF8;
        }
    )");

    auto* pbShadow = new QGraphicsDropShadowEffect(this);
    pbShadow->setBlurRadius(16);
    pbShadow->setColor(QColor(0, 0, 0, 140));
    pbShadow->setOffset(0, 3);
    presetsBar->setGraphicsEffect(pbShadow);

    auto* pbLayout = new QHBoxLayout(presetsBar);
    pbLayout->setContentsMargins(6, 4, 6, 4);
    pbLayout->setSpacing(4);

    auto addPresetBtn = [this, pbLayout](const QString& text, double lat, double lon, int z) {
        auto* btn = new QPushButton(text, presetsBar);
        connect(btn, &QPushButton::clicked, this, [this, lat, lon, z]() {
            setViewport(lat, lon, z);
        });
        pbLayout->addWidget(btn);
    };

    addPresetBtn("Sundarbans", 21.95, 88.90, 10);
    addPresetBtn("Mumbai", 18.92, 72.83, 12);
    addPresetBtn("Gulf of Khambhat", 21.17, 72.83, 11);
    addPresetBtn("Kerala Backwaters", 9.50, 76.34, 12);
    addPresetBtn("Chennai", 13.08, 80.27, 11);
    addPresetBtn("Assam Valley", 26.20, 92.94, 8);
    addPresetBtn("India Overview", 22.00, 79.00, 5);

    // -------------------------------------------------------------
    // 3. Floating Live Probe HUD (Bottom-Left)
    // -------------------------------------------------------------
    probeBadge = new QWidget(this);
    probeBadge->setStyleSheet(R"(
        QWidget {
            background-color: rgba(18, 18, 22, 0.94);
            border: 1px solid #3F3F46;
            border-radius: 8px;
        }
        QLabel {
            font-family: 'Segoe UI', Arial, sans-serif;
            color: #E4E4E7;
            font-size: 11px;
        }
    )");

    auto* prShadow = new QGraphicsDropShadowEffect(this);
    prShadow->setBlurRadius(14);
    prShadow->setColor(QColor(0, 0, 0, 140));
    prShadow->setOffset(0, 3);
    probeBadge->setGraphicsEffect(prShadow);

    auto* prLayout = new QVBoxLayout(probeBadge);
    prLayout->setContentsMargins(10, 8, 10, 8);
    prLayout->setSpacing(3);

    lblProbeCoords = new QLabel("📍 22.0000° N, 79.0000° E", probeBadge);
    lblProbeCoords->setStyleSheet("font-weight: 600; color: #FFFFFF; font-size: 11px;");

    lblProbeElev = new QLabel("⛰️ Ground Elevation: 450.0 m MSL", probeBadge);
    lblProbeElev->setStyleSheet("color: #38BDF8; font-weight: 600; font-size: 11px;");

    lblProbeClearance = new QLabel("🌊 Status: ✓ Safe (+450.0m clearance)", probeBadge);
    lblProbeClearance->setStyleSheet("color: #34D399; font-weight: 600; font-size: 11px;");

    prLayout->addWidget(lblProbeCoords);
    prLayout->addWidget(lblProbeElev);
    prLayout->addWidget(lblProbeClearance);

    // -------------------------------------------------------------
    // 4. Floating Gradient Legend Bar (Bottom-Right)
    // -------------------------------------------------------------
    legendBar = new QWidget(this);
    legendBar->setStyleSheet(R"(
        QWidget {
            background-color: rgba(18, 18, 22, 0.94);
            border: 1px solid #3F3F46;
            border-radius: 8px;
        }
        QLabel {
            font-family: 'Segoe UI', Arial, sans-serif;
            color: #D4D4D8;
            font-size: 9px;
            font-weight: 600;
        }
    )");

    auto* lgShadow = new QGraphicsDropShadowEffect(this);
    lgShadow->setBlurRadius(14);
    lgShadow->setColor(QColor(0, 0, 0, 140));
    lgShadow->setOffset(0, 3);
    legendBar->setGraphicsEffect(lgShadow);

    auto* lgLayout = new QVBoxLayout(legendBar);
    lgLayout->setContentsMargins(10, 8, 10, 8);
    lgLayout->setSpacing(4);

    lblLegendTitle = new QLabel("Elevation Above Sea Level (MSL)", legendBar);
    lblLegendTitle->setStyleSheet("color: #FFFFFF; font-size: 10px; font-weight: bold;");
    lgLayout->addWidget(lblLegendTitle);

    legendGradient = new QWidget(legendBar);
    legendGradient->setFixedSize(220, 12);
    lgLayout->addWidget(legendGradient);

    auto* ticksLayout = new QHBoxLayout();
    ticksLayout->setContentsMargins(0, 0, 0, 0);
    ticksLayout->addWidget(new QLabel("<0m", legendBar));
    ticksLayout->addStretch();
    ticksLayout->addWidget(new QLabel("0m", legendBar));
    ticksLayout->addStretch();
    ticksLayout->addWidget(new QLabel("15m", legendBar));
    ticksLayout->addStretch();
    ticksLayout->addWidget(new QLabel("150m", legendBar));
    ticksLayout->addStretch();
    ticksLayout->addWidget(new QLabel("1000m+", legendBar));
    lgLayout->addLayout(ticksLayout);

    // -------------------------------------------------------------
    // 5. Navigation Controls & Scale Bar
    // -------------------------------------------------------------
    navControls = new NavigationControls(this);
    scaleBar = new ScaleBar(this);

    // -------------------------------------------------------------
    // 6. Status HUD (Bottom-Center)
    // -------------------------------------------------------------
    statusHud = new QWidget(this);
    statusHud->setStyleSheet(R"(
        QWidget {
            background-color: rgba(24, 24, 27, 0.95);
            border: 1px solid #3F3F46;
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

    lblStatusMode = new QLabel("Sea Level Heat Map", statusHud);
    lblStatusMode->setStyleSheet("font-weight: bold; color: #FB923C;");

    lblStatusCoords = new QLabel("22.0000° N, 79.0000° E", statusHud);
    lblStatusZoom = new QLabel("Zoom: 5", statusHud);
    lblStatusFps = new QLabel("60 FPS", statusHud);
    lblStatusFps->setStyleSheet("font-weight: bold; color: #81C995;");

    hudLayout->addWidget(lblStatusMode);
    hudLayout->addWidget(lblStatusCoords);
    hudLayout->addWidget(lblStatusZoom);
    hudLayout->addWidget(lblStatusFps);
}

void SeaLevelHeatMapWidget::setupConnections() {
    // Sea Level Slider
    connect(sldSeaLevel, &QSlider::valueChanged, this, [this](int val) {
        double m = val / 10.0;
        lblSeaLevelVal->setText(QString("+%1 m MSL").arg(m, 0, 'f', 1));

        if (m == 0.0) {
            lblRiskTag->setText("● BASELINE MEAN SEA LEVEL");
            lblRiskTag->setStyleSheet("font-size: 10px; font-weight: bold; color: #9CA3AF;");
        } else if (m <= 1.5) {
            lblRiskTag->setText("● IPCC PROJECTED WARMING (2050-2100)");
            lblRiskTag->setStyleSheet("font-size: 10px; font-weight: bold; color: #38BDF8;");
        } else if (m <= 3.5) {
            lblRiskTag->setText("● HIGH TIDE & MONSOON SURGE");
            lblRiskTag->setStyleSheet("font-size: 10px; font-weight: bold; color: #F59E0B;");
        } else if (m <= 6.0) {
            lblRiskTag->setText("● SEVERE CYCLONIC STORM SURGE");
            lblRiskTag->setStyleSheet("font-size: 10px; font-weight: bold; color: #F97316;");
        } else {
            lblRiskTag->setText("● CATASTROPHIC COASTAL TSUNAMI");
            lblRiskTag->setStyleSheet("font-size: 10px; font-weight: bold; color: #EF4444;");
        }

        mapWidget->setSeaLevelRise(m);
        updateMetrics();
    });

    // Simulation Play/Pause
    connect(btnPlaySim, &QPushButton::clicked, this, [this]() {
        if (mapWidget->isSimulating()) {
            mapWidget->pauseRiseSimulation();
            btnPlaySim->setText(" ▶ Play Tide Sim");
            btnPlaySim->setStyleSheet("background-color: #059669; color: #FFFFFF; border-radius: 5px; font-size: 11px; font-weight: 600; padding: 5px 10px;");
        } else {
            mapWidget->startRiseSimulation();
            btnPlaySim->setText(" ⏸ Pause Sim");
            btnPlaySim->setStyleSheet("background-color: #D97706; color: #FFFFFF; border-radius: 5px; font-size: 11px; font-weight: 600; padding: 5px 10px;");
        }
    });

    connect(btnResetSim, &QPushButton::clicked, this, [this]() {
        mapWidget->resetRiseSimulation();
        sldSeaLevel->setValue(0);
        btnPlaySim->setText(" ▶ Play Tide Sim");
        btnPlaySim->setStyleSheet("background-color: #059669; color: #FFFFFF; border-radius: 5px; font-size: 11px; font-weight: 600; padding: 5px 10px;");
    });

    connect(mapWidget, &SeaLevelTileWidget::seaLevelRiseChanged, this, [this](double meters) {
        if (sldSeaLevel->value() != static_cast<int>(meters * 10.0)) {
            sldSeaLevel->blockSignals(true);
            sldSeaLevel->setValue(static_cast<int>(meters * 10.0));
            sldSeaLevel->blockSignals(false);
            lblSeaLevelVal->setText(QString("+%1 m MSL").arg(meters, 0, 'f', 1));
            updateMetrics();
        }
    });

    // Visual Palette & Opacity
    connect(cmbPalette, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        mapWidget->setPalette(static_cast<MapCore::HeatMapPalette>(idx));
        updateLegendGradient();
    });

    connect(sldOpacity, &QSlider::valueChanged, this, [this](int val) {
        mapWidget->setHeatOpacity(val / 100.0f);
    });

    connect(chkContours, &QCheckBox::toggled, mapWidget, &SeaLevelTileWidget::setShowContours);
    connect(chkRipples, &QCheckBox::toggled, mapWidget, &SeaLevelTileWidget::setShowSurgeRipples);
    connect(chkHotspots, &QCheckBox::toggled, mapWidget, &SeaLevelTileWidget::setShowHotspots);

    // Probe HUD
    connect(mapWidget, &SeaLevelTileWidget::cursorElevationProbe, this, [this](double lat, double lon, double elev, double clearance) {
        lblProbeCoords->setText(QString("📍 %1° N, %2° E").arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4));
        lblProbeElev->setText(QString("⛰️ Ground Elevation: %1 m MSL").arg(elev, 0, 'f', 1));

        if (elev <= mapWidget->getSeaLevelRise()) {
            lblProbeClearance->setText(QString("🌊 Status: ⚠️ SUBMERGED (-%1m)").arg(std::abs(clearance), 0, 'f', 1));
            lblProbeClearance->setStyleSheet("color: #F43F5E; font-weight: 600; font-size: 11px;");
        } else if (clearance <= 3.0) {
            lblProbeClearance->setText(QString("🌊 Status: ⚠️ HIGH HAZARD (+%1m clearance)").arg(clearance, 0, 'f', 1));
            lblProbeClearance->setStyleSheet("color: #F59E0B; font-weight: 600; font-size: 11px;");
        } else {
            lblProbeClearance->setText(QString("🌊 Status: ✓ SECURE (+%1m clearance)").arg(clearance, 0, 'f', 1));
            lblProbeClearance->setStyleSheet("color: #34D399; font-weight: 600; font-size: 11px;");
        }
    });

    // Viewport synchronization
    connect(mapWidget, &OnlineTileWidget::viewportChanged, this, [this](double lat, double lon, int zoom) {
        lblStatusCoords->setText(QString("%1° N, %2° E").arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4));
        lblStatusZoom->setText(QString("Zoom: %1").arg(zoom));
        scaleBar->updateScale(static_cast<float>(zoom), lat, width());
        updateMetrics();
        emit viewportChanged(lat, lon, zoom);
    });

    connect(mapWidget, &OnlineTileWidget::contextMenuRequested, this, &SeaLevelHeatMapWidget::contextMenuRequested);

    // Navigation Controls
    connect(navControls, &NavigationControls::zoomInRequested, this, &SeaLevelHeatMapWidget::zoomIn);
    connect(navControls, &NavigationControls::zoomOutRequested, this, &SeaLevelHeatMapWidget::zoomOut);
    connect(navControls, &NavigationControls::fitExtentRequested, this, &SeaLevelHeatMapWidget::fitIndia);
    connect(navControls, &NavigationControls::resetNorthRequested, this, &SeaLevelHeatMapWidget::fitIndia);
}

void SeaLevelHeatMapWidget::setViewport(double lat, double lon, int zoom) {
    if (mapWidget) {
        mapWidget->setCenter(lat, lon);
        mapWidget->setZoom(zoom);
    }
}

void SeaLevelHeatMapWidget::setDarkMode(bool isDark) {
    if (mapWidget) {
        mapWidget->setDarkMode(isDark);
    }
}

void SeaLevelHeatMapWidget::updateMetrics() {
    if (!mapWidget) return;
    double area = mapWidget->calculateInundatedAreaKm2();
    double pop = mapWidget->calculateVulnerablePopulationMillions();

    lblInundatedArea->setText(QString("Inundated Land: %1 km²").arg(QLocale(QLocale::English).toString(static_cast<int>(area))));
    lblVulnerablePop->setText(QString("Vulnerable Pop: %1 M").arg(pop, 0, 'f', 2));
}

void SeaLevelHeatMapWidget::updateLegendGradient() {
    auto pal = static_cast<MapCore::HeatMapPalette>(cmbPalette->currentIndex());
    QString grad;

    if (pal == MapCore::HeatMapPalette::SeaLevelInundation) {
        grad = "qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #001F3F, stop:0.2 #00F0FF, stop:0.4 #10B981, stop:0.6 #FACC15, stop:0.8 #EA580C, stop:1.0 #FFFFFF)";
    } else if (pal == MapCore::HeatMapPalette::Turbo) {
        grad = "qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #30123B, stop:0.25 #1AE4B6, stop:0.5 #A2FC3C, stop:0.75 #FABA39, stop:1.0 #7A0403)";
    } else if (pal == MapCore::HeatMapPalette::Magma) {
        grad = "qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #000004, stop:0.3 #51127C, stop:0.6 #B73779, stop:0.85 #FB8861, stop:1.0 #FCFDBF)";
    } else if (pal == MapCore::HeatMapPalette::OceanDepth) {
        grad = "qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #020617, stop:0.3 #0369A1, stop:0.6 #06B6D4, stop:0.85 #67E8F9, stop:1.0 #E0F2FE)";
    } else {
        grad = "qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #440154, stop:0.25 #3B528B, stop:0.5 #21918C, stop:0.75 #5EC962, stop:1.0 #FDE725)";
    }

    legendGradient->setStyleSheet(QString("background: %1; border-radius: 3px; border: 1px solid rgba(255,255,255,0.2);").arg(grad));
}

void SeaLevelHeatMapWidget::resizeEvent(QResizeEvent* /*event*/) {
    updateFloatingPositions();
}

bool SeaLevelHeatMapWidget::eventFilter(QObject* watched, QEvent* event) {
    if (watched == mapWidget && event->type() == QEvent::Resize) {
        updateFloatingPositions();
    }
    return QWidget::eventFilter(watched, event);
}

void SeaLevelHeatMapWidget::updateFloatingPositions() {
    int w = width();
    int h = height();
    if (w < 100 || h < 100) return;

    // 1. Control Panel at Top-Left
    controlCard->move(20, 20);

    // 2. Presets Bar at Top-Right
    presetsBar->adjustSize();
    presetsBar->move(w - presetsBar->width() - 20, 20);

    // 3. Navigation Controls at Bottom-Right
    int navW = navControls->sizeHint().width();
    int navH = navControls->sizeHint().height();
    navControls->setGeometry(w - navW - 20, h - navH - 45, navW, navH);

    // 4. Scale Bar at Bottom-Right (left of nav controls)
    scaleBar->move(w - navW - 20 - scaleBar->width() - 15, h - 50);

    // 5. Legend Bar at Bottom-Right (above navigation controls)
    legendBar->adjustSize();
    legendBar->move(w - legendBar->width() - 20, h - navH - legendBar->height() - 55);

    // 6. Live Probe HUD at Bottom-Left
    probeBadge->adjustSize();
    probeBadge->move(20, h - probeBadge->height() - 45);

    // 7. Status HUD at Bottom-Center
    statusHud->adjustSize();
    statusHud->move((w - statusHud->width()) / 2, h - statusHud->height() - 8);
}

} // namespace MapUI
