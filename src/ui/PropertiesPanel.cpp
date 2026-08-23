#include "PropertiesPanel.h"
#include "IconHelper.h"
#include "../core/DamManager.h"

namespace MapUI {

// ==========================================
// CollapsibleSection Implementation
// ==========================================

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 2, 0, 4);
    mainLayout->setSpacing(0);

    // Section Header Button with SVG Chevron
    btnHeader = new QPushButton(" " + title, this);
    btnHeader->setIcon(IconHelper::chevronDown(QColor(220, 220, 225), 14));
    btnHeader->setIconSize(QSize(14, 14));
    btnHeader->setStyleSheet(R"(
        QPushButton {
            background-color: #1E1E22;
            color: #FFFFFF;
            border: 1px solid #27272A;
            border-radius: 4px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: 600;
            text-align: left;
            padding: 6px 10px;
        }
        QPushButton:hover {
            background-color: #27272A;
            color: #FFFFFF;
        }
    )");

    contentWidget = new QWidget(this);
    contentWidget->setStyleSheet(R"(
        QWidget {
            background-color: transparent;
            border: none;
        }
    )");

    contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(8, 6, 8, 6);
    contentLayout->setSpacing(6);

    mainLayout->addWidget(btnHeader);
    mainLayout->addWidget(contentWidget);

    connect(btnHeader, &QPushButton::clicked, this, &CollapsibleSection::toggleCollapse);
}

void CollapsibleSection::addWidget(QWidget* w) {
    contentLayout->addWidget(w);
}

void CollapsibleSection::toggleCollapse() {
    isCollapsed = !isCollapsed;
    contentWidget->setVisible(!isCollapsed);
    btnHeader->setIcon(isCollapsed ? IconHelper::chevronRight(QColor(200, 200, 205), 14)
                                  : IconHelper::chevronDown(QColor(200, 200, 205), 14));
}

// ==========================================
// PropertiesPanel Implementation
// ==========================================

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void PropertiesPanel::setupUi() {
    setMinimumWidth(300);

    setStyleSheet(R"(
        QWidget#propertiesPanel {
            background-color: #18181B;
            border-left: 1px solid #27272A;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
        }
        QWidget#iconStrip {
            background-color: #121214;
            border-right: 1px solid #27272A;
        }
        QWidget#iconStrip QPushButton {
            background-color: transparent;
            border: none;
            border-left: 3px solid transparent;
            border-radius: 0px;
            min-width: 40px;
            max-width: 40px;
            min-height: 40px;
            max-height: 40px;
        }
        QWidget#iconStrip QPushButton:hover {
            background-color: #27272A;
        }
        QWidget#iconStrip QPushButton:checked {
            background-color: #27272A;
            border-left: 3px solid #38BDF8;
        }
        QScrollArea {
            border: none;
            background-color: #18181B;
        }
        QScrollBar:vertical {
            background: #121214;
            width: 6px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3F3F46;
            min-height: 20px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: #52525B;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QLabel {
            color: #FFFFFF;
            background: transparent;
            border: none;
        }
        QDoubleSpinBox, QSlider {
            color: #FFFFFF;
        }
        QDoubleSpinBox {
            background-color: #18181B;
            border: 1px solid #27272A;
            border-radius: 4px;
            padding: 4px 6px;
            color: #FFFFFF;
            font-family: Consolas, monospace;
            font-size: 11px;
        }
        QDoubleSpinBox:focus {
            border-color: #38BDF8;
        }
        QCheckBox {
            color: #FFFFFF;
            font-size: 11px;
            spacing: 6px;
            background: transparent;
            border: none;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            background-color: #18181B;
            border: 1px solid #3F3F46;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked {
            background-color: #38BDF8;
            border-color: #38BDF8;
        }
        QPushButton#btnBake {
            background-color: #0284C7;
            color: #FFFFFF;
            border: 1px solid #0369A1;
            border-radius: 4px;
            font-size: 11px;
            font-weight: 600;
            padding: 7px 12px;
        }
        QPushButton#btnBake:hover {
            background-color: #0369A1;
        }
        QPushButton#btnActionBlue {
            background-color: #27272A;
            color: #FFFFFF;
            border: 1px solid #3F3F46;
            border-radius: 4px;
            font-size: 11px;
            font-weight: 600;
            padding: 6px 10px;
        }
        QPushButton#btnActionBlue:hover {
            background-color: #3F3F46;
        }
        QPushButton#btnReset {
            background-color: #27272A;
            color: #FFFFFF;
            border: 1px solid #3F3F46;
            border-radius: 4px;
            font-size: 11px;
            padding: 6px 12px;
        }
        QPushButton#btnReset:hover {
            background-color: #3F3F46;
            color: #FFFFFF;
        }
    )");

    setObjectName("propertiesPanel");

    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Vertical Icon Tab Bar (Left Edge of Sidebar)
    iconStrip = new QWidget(this);
    iconStrip->setObjectName("iconStrip");
    iconStrip->setFixedWidth(42);

    auto* isLayout = new QVBoxLayout(iconStrip);
    isLayout->setContentsMargins(0, 4, 0, 4);
    isLayout->setSpacing(2);

    tabButtonGroup = new QButtonGroup(this);
    tabButtonGroup->setExclusive(true);

    btnTabFluid = new QPushButton(iconStrip);
    btnTabFluid->setIcon(IconHelper::rain(QColor(138, 180, 248), 22));
    btnTabFluid->setIconSize(QSize(22, 22));
    btnTabFluid->setCheckable(true);
    btnTabFluid->setToolTip("Fluid & Hydrodynamics");
    tabButtonGroup->addButton(btnTabFluid, 0);
    isLayout->addWidget(btnTabFluid);

    btnTabTerrain = new QPushButton(iconStrip);
    btnTabTerrain->setIcon(IconHelper::sunFog(QColor(129, 201, 149), 22));
    btnTabTerrain->setIconSize(QSize(22, 22));
    btnTabTerrain->setCheckable(true);
    btnTabTerrain->setToolTip("Terrain & Embankments");
    tabButtonGroup->addButton(btnTabTerrain, 1);
    isLayout->addWidget(btnTabTerrain);

    btnTabTelemetry = new QPushButton(iconStrip);
    btnTabTelemetry->setIcon(IconHelper::radar(QColor(253, 214, 99), 22));
    btnTabTelemetry->setIconSize(QSize(22, 22));
    btnTabTelemetry->setCheckable(true);
    btnTabTelemetry->setToolTip("Assam River Gauges Telemetry");
    tabButtonGroup->addButton(btnTabTelemetry, 2);
    isLayout->addWidget(btnTabTelemetry);

    btnTabDisplay = new QPushButton(iconStrip);
    btnTabDisplay->setIcon(IconHelper::map(QColor(167, 139, 250), 22));
    btnTabDisplay->setIconSize(QSize(22, 22));
    btnTabDisplay->setCheckable(true);
    btnTabDisplay->setToolTip("Viewport Overlays & Heatmaps");
    tabButtonGroup->addButton(btnTabDisplay, 3);
    isLayout->addWidget(btnTabDisplay);

    btnTabDam = new QPushButton(iconStrip);
    btnTabDam->setIcon(IconHelper::info(QColor(84, 213, 154), 22));
    btnTabDam->setIconSize(QSize(22, 22));
    btnTabDam->setCheckable(true);
    btnTabDam->setToolTip("Dam Specifications & Preferences");
    tabButtonGroup->addButton(btnTabDam, 4);
    isLayout->addWidget(btnTabDam);

    isLayout->addStretch();

    mainLayout->addWidget(iconStrip);

    // 2. Right Content Area (Header + Stacked Pages)
    auto* contentArea = new QWidget(this);
    auto* caLayout = new QVBoxLayout(contentArea);
    caLayout->setContentsMargins(0, 0, 0, 0);
    caLayout->setSpacing(0);

    // Header Strip
    auto* topHeader = new QWidget(contentArea);
    topHeader->setStyleSheet("background-color: #242424; border-bottom: 1px solid #1D1D1D;");
    auto* thLayout = new QHBoxLayout(topHeader);
    thLayout->setContentsMargins(12, 8, 12, 8);

    lblPanelTitle = new QLabel("Fluid & Hydrodynamics", topHeader);
    lblPanelTitle->setStyleSheet("color: #FFFFFF; font-weight: bold; font-size: 12px;");
    thLayout->addWidget(lblPanelTitle);
    thLayout->addStretch();

    caLayout->addWidget(topHeader);

    // Page Stack
    pageStack = new QStackedWidget(contentArea);
    pageStack->addWidget(createFluidTab());
    pageStack->addWidget(createTerrainTab());
    pageStack->addWidget(createTelemetryTab());
    pageStack->addWidget(createDisplayTab());
    pageStack->addWidget(createDamTab());

    caLayout->addWidget(pageStack, 1);
    mainLayout->addWidget(contentArea, 1);

    // Connect Tab Buttons
    connect(btnTabFluid, &QPushButton::clicked, this, [this]() {
        switchTab(0, "Fluid & Hydrodynamics");
    });
    connect(btnTabTerrain, &QPushButton::clicked, this, [this]() {
        switchTab(1, "Terrain & Embankments");
    });
    connect(btnTabTelemetry, &QPushButton::clicked, this, [this]() {
        switchTab(2, "Assam River Gauges Telemetry");
    });
    connect(btnTabDisplay, &QPushButton::clicked, this, [this]() {
        switchTab(3, "Viewport Overlays & Heatmaps");
    });
    connect(btnTabDam, &QPushButton::clicked, this, [this]() {
        switchTab(4, "Dam Specifications & Preferences");
    });

    btnTabFluid->setChecked(true);
    switchTab(0, "Fluid & Hydrodynamics");
}

void PropertiesPanel::switchTab(int index, const QString& title) {
    if (pageStack && index >= 0 && index < pageStack->count()) {
        pageStack->setCurrentIndex(index);
    }
    if (lblPanelTitle) {
        lblPanelTitle->setText(title);
    }
}

QWidget* PropertiesPanel::createFluidTab() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Section 1: Inflow & Boundary Conditions
    auto* secInflow = new CollapsibleSection("Inflow & Hydrodynamics", container);

    auto makeRow = [](const QString& labelText, QWidget* controlWidget) {
        auto* row = new QWidget();
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(labelText, row);
        lbl->setMinimumWidth(110);
        hl->addWidget(lbl);
        hl->addWidget(controlWidget, 1);
        return row;
    };

    spinWaterRise = new QDoubleSpinBox(container);
    spinWaterRise->setRange(0.0, 25.0);
    spinWaterRise->setValue(3.5);
    spinWaterRise->setSingleStep(0.5);
    spinWaterRise->setSuffix(" m");
    secInflow->addWidget(makeRow("Peak Water Rise:", spinWaterRise));

    spinRainfall = new QDoubleSpinBox(container);
    spinRainfall->setRange(0.0, 500.0);
    spinRainfall->setValue(120.0);
    spinRainfall->setSingleStep(10.0);
    spinRainfall->setSuffix(" mm/h");
    secInflow->addWidget(makeRow("Rainfall Intensity:", spinRainfall));

    spinBreachWidth = new QDoubleSpinBox(container);
    spinBreachWidth->setRange(5.0, 500.0);
    spinBreachWidth->setValue(75.0);
    spinBreachWidth->setSingleStep(5.0);
    spinBreachWidth->setSuffix(" m");
    secInflow->addWidget(makeRow("Breach Width:", spinBreachWidth));

    spinVelocity = new QDoubleSpinBox(container);
    spinVelocity->setRange(0.1, 15.0);
    spinVelocity->setValue(2.8);
    spinVelocity->setSingleStep(0.2);
    spinVelocity->setSuffix(" m/s");
    secInflow->addWidget(makeRow("Flow Velocity:", spinVelocity));

    layout->addWidget(secInflow);

    // Section 2: Physics Simulation Engine
    auto* secPhysics = new CollapsibleSection("Bake Physics Simulation", container);

    btnBakeSim = new QPushButton("Bake 2D Fluid Mesh", container);
    btnBakeSim->setObjectName("btnBake");
    secPhysics->addWidget(btnBakeSim);

    btnResetSim = new QPushButton("Reset Simulation", container);
    btnResetSim->setObjectName("btnReset");
    secPhysics->addWidget(btnResetSim);

    connect(btnBakeSim, &QPushButton::clicked, this, [this]() {
        emit simulationBakeRequested(spinWaterRise->value(), spinRainfall->value(), spinBreachWidth->value());
    });

    layout->addWidget(secPhysics);

    layout->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* PropertiesPanel::createTerrainTab() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* secManning = new CollapsibleSection("Surface Roughness (Manning's n)", container);
    auto makeRow = [](const QString& labelText, QWidget* controlWidget) {
        auto* row = new QWidget();
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(labelText, row);
        lbl->setMinimumWidth(110);
        hl->addWidget(lbl);
        hl->addWidget(controlWidget, 1);
        return row;
    };

    auto* spinRiverBed = new QDoubleSpinBox(container);
    spinRiverBed->setRange(0.015, 0.08);
    spinRiverBed->setValue(0.030);
    spinRiverBed->setSingleStep(0.005);
    secManning->addWidget(makeRow("River Channel:", spinRiverBed));

    auto* spinFloodplain = new QDoubleSpinBox(container);
    spinFloodplain->setRange(0.025, 0.15);
    spinFloodplain->setValue(0.055);
    spinFloodplain->setSingleStep(0.005);
    secManning->addWidget(makeRow("Floodplain Vegetated:", spinFloodplain));

    layout->addWidget(secManning);

    auto* secEmbankments = new CollapsibleSection("Assam Embankments & Dykes", container);
    auto* chkDykes = new QCheckBox("Enforce Brahmaputra Dykes", container);
    chkDykes->setChecked(true);
    auto* chkBreachPoints = new QCheckBox("Highlight Vulnerable Breach Points", container);
    chkBreachPoints->setChecked(true);

    secEmbankments->addWidget(chkDykes);
    secEmbankments->addWidget(chkBreachPoints);
    layout->addWidget(secEmbankments);

    layout->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* PropertiesPanel::createTelemetryTab() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* secStations = new CollapsibleSection("Assam River Gauges (Live)", container);

    auto makeGauge = [](const QString& name, const QString& level, const QString& status) {
        auto* card = new QWidget();
        card->setStyleSheet("background: transparent; border-bottom: 1px solid #27272A;");
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(4, 4, 4, 4);
        cl->setSpacing(2);

        auto* top = new QHBoxLayout();
        auto* lblName = new QLabel(name, card);
        lblName->setStyleSheet("font-weight: 600; color: #FFFFFF; font-size: 11px; background: transparent; border: none;");
        auto* lblStatus = new QLabel(status, card);
        lblStatus->setStyleSheet("color: #FFFFFF; font-size: 10px; background: transparent; border: none;");
        top->addWidget(lblName);
        top->addStretch();
        top->addWidget(lblStatus);

        auto* lblLevel = new QLabel("Current Level: " + level, card);
        lblLevel->setStyleSheet("color: #FFFFFF; font-size: 10px; font-family: Consolas, monospace; background: transparent; border: none;");

        cl->addLayout(top);
        cl->addWidget(lblLevel);
        return card;
    };

    secStations->addWidget(makeGauge("Guwahati (Brahmaputra)", "49.68 m", "ALERT (+0.68m)"));
    secStations->addWidget(makeGauge("Dibrugarh", "104.24 m", "NORMAL"));
    secStations->addWidget(makeGauge("Tezpur", "64.20 m", "NORMAL"));
    secStations->addWidget(makeGauge("Dhubri", "29.10 m", "WARNING"));
    secStations->addWidget(makeGauge("Nematighat (Jorhat)", "85.90 m", "HIGH"));

    layout->addWidget(secStations);
    layout->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* PropertiesPanel::createDisplayTab() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* secOverlay = new CollapsibleSection("Viewport Overlays", container);
    auto* chkInundation = new QCheckBox("Inundation Depth Color Heatmap", container);
    chkInundation->setChecked(true);
    auto* chkVectors = new QCheckBox("Water Velocity Vectors", container);
    chkVectors->setChecked(true);
    auto* chkRiskPOI = new QCheckBox("Critical Facilities at Risk", container);
    chkRiskPOI->setChecked(true);
    auto* chkEvacRoutes = new QCheckBox("Evacuation Highway Corridors", container);
    chkEvacRoutes->setChecked(true);

    secOverlay->addWidget(chkInundation);
    secOverlay->addWidget(chkVectors);
    secOverlay->addWidget(chkRiskPOI);
    secOverlay->addWidget(chkEvacRoutes);
    layout->addWidget(secOverlay);

    layout->addStretch();
    scroll->setWidget(container);
    return scroll;
}

QWidget* PropertiesPanel::createDamTab() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 1. Dam Header Card
    auto* headerCard = new QWidget(container);
    headerCard->setStyleSheet("background: transparent; border: none;");
    auto* hcLayout = new QVBoxLayout(headerCard);
    hcLayout->setContentsMargins(4, 4, 4, 4);
    hcLayout->setSpacing(4);

    lblDamName = new QLabel("Select a Dam on Map", headerCard);
    lblDamName->setStyleSheet("color: #FFFFFF; font-size: 13px; font-weight: bold; background: transparent; border: none;");
    lblDamName->setWordWrap(true);

    lblDamPic = new QLabel("National PIC: --", headerCard);
    lblDamPic->setStyleSheet("color: #FFFFFF; font-size: 11px; background: transparent; border: none;");

    lblDamStatus = new QLabel("National Hydrological Asset", headerCard);
    lblDamStatus->setStyleSheet("color: #FFFFFF; font-size: 11px; background: transparent; border: none;");

    hcLayout->addWidget(lblDamName);
    hcLayout->addWidget(lblDamPic);
    hcLayout->addWidget(lblDamStatus);
    layout->addWidget(headerCard);

    // 2. Geographic & Regional Details
    auto* secGeo = new CollapsibleSection("Geographic & Regional Details", container);
    auto makeDataRow = [](const QString& labelText, QLabel*& valueLabel) {
        auto* row = new QWidget();
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(labelText, row);
        lbl->setStyleSheet("color: #D4D4D8; font-size: 11px; background: transparent; border: none;");
        lbl->setMinimumWidth(110);
        valueLabel = new QLabel("--", row);
        valueLabel->setStyleSheet("color: #FFFFFF; font-size: 11px; font-weight: normal; background: transparent; border: none;");
        valueLabel->setWordWrap(true);
        hl->addWidget(lbl);
        hl->addWidget(valueLabel, 1);
        return row;
    };

    secGeo->addWidget(makeDataRow("State:", lblDamState));
    secGeo->addWidget(makeDataRow("District:", lblDamDistrict));
    secGeo->addWidget(makeDataRow("River:", lblDamRiver));
    secGeo->addWidget(makeDataRow("River Basin:", lblDamBasin));
    secGeo->addWidget(makeDataRow("Coordinates:", lblDamCoords));
    secGeo->addWidget(makeDataRow("Authority:", lblDamIncharge));
    layout->addWidget(secGeo);

    // 3. Technical & Structural Engineering Specifications
    auto* secEng = new CollapsibleSection("Engineering Specifications", container);
    secEng->addWidget(makeDataRow("Structure Type:", lblDamType));
    secEng->addWidget(makeDataRow("Structural Height:", lblDamHeight));
    secEng->addWidget(makeDataRow("Gross Storage:", lblDamStorage));
    secEng->addWidget(makeDataRow("Discharge Capacity:", lblDamSpillway));
    secEng->addWidget(makeDataRow("Commissioned:", lblDamYear));
    secEng->addWidget(makeDataRow("Primary Purpose:", lblDamPurpose));
    layout->addWidget(secEng);

    // 4. Live Hydrodynamic Wave Propagation (Physical Simulation Data)
    auto* secHydro = new CollapsibleSection("Hydrodynamic Wave Propagation (60 min)", container);

    secHydro->addWidget(makeDataRow("Time Elapsed:", lblHydroTime));
    secHydro->addWidget(makeDataRow("Active Depression:", lblHydroBasin));
    secHydro->addWidget(makeDataRow("Bed / Saddle Lip:", lblHydroElev));
    secHydro->addWidget(makeDataRow("Water Surface (WSE):", lblHydroWSE));
    secHydro->addWidget(makeDataRow("Saddle Spill Status:", lblHydroSpillStatus));
    secHydro->addWidget(makeDataRow("Depression Storage:", lblHydroPondVol));
    secHydro->addWidget(makeDataRow("Inundated Area:", lblHydroArea));
    secHydro->addWidget(makeDataRow("Wave Front Dist:", lblHydroFront));
    secHydro->addWidget(makeDataRow("Peak Water Depth:", lblHydroDepth));
    secHydro->addWidget(makeDataRow("Flow Velocity:", lblHydroVel));
    secHydro->addWidget(makeDataRow("Peak Discharge Q:", lblHydroDischarge));
    layout->addWidget(secHydro);

    // 5. Dam Preferences & Simulation Tuning
    auto* secPref = new CollapsibleSection("Preferences & Simulation", container);

    auto makeSpinRow = [](const QString& labelText, QWidget* controlWidget) {
        auto* row = new QWidget();
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(labelText, row);
        lbl->setStyleSheet("color: #D4D4D8; font-size: 11px; background: transparent; border: none;");
        lbl->setMinimumWidth(120);
        hl->addWidget(lbl);
        hl->addWidget(controlWidget, 1);
        return row;
    };

    spinDamWarningThreshold = new QDoubleSpinBox(container);
    spinDamWarningThreshold->setRange(10.0, 100.0);
    spinDamWarningThreshold->setValue(80.0);
    spinDamWarningThreshold->setSingleStep(5.0);
    spinDamWarningThreshold->setSuffix(" %");
    secPref->addWidget(makeSpinRow("Alert Threshold:", spinDamWarningThreshold));

    spinDamInflowMultiplier = new QDoubleSpinBox(container);
    spinDamInflowMultiplier->setRange(0.1, 10.0);
    spinDamInflowMultiplier->setValue(1.0);
    spinDamInflowMultiplier->setSingleStep(0.2);
    spinDamInflowMultiplier->setSuffix(" x");
    secPref->addWidget(makeSpinRow("Inflow Multiplier:", spinDamInflowMultiplier));

    btnCenterOnDam = new QPushButton("🎯 Center Map on Dam", container);
    btnCenterOnDam->setObjectName("btnActionBlue");
    secPref->addWidget(btnCenterOnDam);

    btnSimulateDam = new QPushButton("🌊 Run Localized Inundation", container);
    btnSimulateDam->setObjectName("btnBake");
    secPref->addWidget(btnSimulateDam);

    connect(btnCenterOnDam, &QPushButton::clicked, this, [this]() {
        if (std::abs(currentDamLat) > 0.001 || std::abs(currentDamLon) > 0.001) {
            emit centerLocationRequested(currentDamLat, currentDamLon, 12);
        }
    });

    connect(btnSimulateDam, &QPushButton::clicked, this, [this]() {
        emit simulationBakeRequested(spinWaterRise ? spinWaterRise->value() : 3.5,
                                    spinRainfall ? spinRainfall->value() : 120.0,
                                    spinBreachWidth ? spinBreachWidth->value() : 75.0);
    });

    layout->addWidget(secPref);

    layout->addStretch();
    scroll->setWidget(container);
    return scroll;
}

void PropertiesPanel::showDamDetails(const MapCore::DamPoint& dam) {
    currentDamLat = dam.lat;
    currentDamLon = dam.lon;

    if (lblDamName) lblDamName->setText(dam.name.isEmpty() ? "Unnamed Dam / Reservoir" : dam.name);
    if (lblDamPic) lblDamPic->setText(QString("National PIC: %1").arg(dam.pic.isEmpty() ? "N/A" : dam.pic));
    if (lblDamStatus) lblDamStatus->setText("● National Hydrological Asset");

    if (lblDamState) lblDamState->setText(QString("State: %1").arg(dam.state.isEmpty() ? "India" : dam.state));
    if (lblDamDistrict) lblDamDistrict->setText(QString("District: %1").arg(dam.district.isEmpty() ? "N/A" : dam.district));
    if (lblDamRiver) lblDamRiver->setText(QString("River: %1").arg(dam.river.isEmpty() ? "N/A" : dam.river));
    if (lblDamBasin) lblDamBasin->setText(QString("Basin: %1").arg(dam.basin.isEmpty() ? "N/A" : dam.basin));
    if (lblDamCoords) lblDamCoords->setText(QString("%1° N, %2° E").arg(dam.lat, 0, 'f', 4).arg(dam.lon, 0, 'f', 4));
    if (lblDamIncharge) lblDamIncharge->setText(dam.incharge.isEmpty() ? "State Water Resources Dept." : dam.incharge);

    if (lblDamType) lblDamType->setText(dam.damType.isEmpty() ? "Earthen / Gravity Structure" : dam.damType);
    if (lblDamHeight) lblDamHeight->setText(dam.height > 0 ? QString("%1 m").arg(dam.height, 0, 'f', 1) : "Data pending");
    if (lblDamStorage) lblDamStorage->setText(dam.storage > 0 ? QString("%1 MCM").arg(dam.storage, 0, 'f', 1) : "Data pending");
    if (lblDamSpillway) lblDamSpillway->setText(dam.spillwayCap > 0 ? QString("%1 m³/s").arg(dam.spillwayCap, 0, 'f', 1) : "Data pending");
    if (lblDamYear) lblDamYear->setText(dam.year > 0 ? QString("%1").arg(dam.year) : "Historical");
    if (lblDamPurpose) lblDamPurpose->setText(dam.purpose.isEmpty() ? "Irrigation & Flood Moderation" : dam.purpose);

    btnTabDam->setChecked(true);
    switchTab(4, "Dam Specifications & Preferences");
}

void PropertiesPanel::showDamSelectionSummary(int count, double minLat, double minLon, double maxLat, double maxLon, const std::vector<const MapCore::DamPoint*>& dams) {
    if (count == 1 && !dams.empty() && dams[0]) {
        showDamDetails(*dams[0]);
        return;
    }

    currentDamLat = (minLat + maxLat) / 2.0;
    currentDamLon = (minLon + maxLon) / 2.0;

    if (lblDamName) lblDamName->setText(QString("Multi-Selection (%1 Dams)").arg(count));
    if (lblDamPic) lblDamPic->setText(QString("Region: [%1°N, %2°E] to [%3°N, %4°E]").arg(minLat, 0, 'f', 2).arg(minLon, 0, 'f', 2).arg(maxLat, 0, 'f', 2).arg(maxLon, 0, 'f', 2));
    if (lblDamStatus) lblDamStatus->setText(QString("● %1 Infrastructure Assets Selected").arg(count));

    if (lblDamState) lblDamState->setText(QString("%1 dams selected").arg(count));
    if (lblDamDistrict) lblDamDistrict->setText(QString("Span Lat: %1°").arg(maxLat - minLat, 0, 'f', 3));
    if (lblDamRiver) lblDamRiver->setText(QString("Span Lon: %1°").arg(maxLon - minLon, 0, 'f', 3));
    if (lblDamBasin) lblDamBasin->setText("Regional Bounding Box");
    if (lblDamCoords) lblDamCoords->setText(QString("Center: %1° N, %2° E").arg(currentDamLat, 0, 'f', 4).arg(currentDamLon, 0, 'f', 4));
    if (lblDamIncharge) lblDamIncharge->setText("Multiple Authorities");

    if (lblDamType) lblDamType->setText("Multiple Structural Types");
    if (lblDamHeight) lblDamHeight->setText("Regional Cluster");
    if (lblDamStorage) lblDamStorage->setText("Combined Storage Basin");
    if (lblDamSpillway) lblDamSpillway->setText("Multiple Spillways");
    if (lblDamYear) lblDamYear->setText("Multi-Era");
    if (lblDamPurpose) lblDamPurpose->setText("Irrigation, Power & Flood Mitigation");

    btnTabDam->setChecked(true);
    switchTab(4, "Dam Selection & Regional Analysis");
}

void PropertiesPanel::updateHydrodynamicPropagation(int minute, double areaKm2, double frontDistKm, double maxDepthM, double maxVelMs, double peakDischargeQ,
                                                   const QString& basinName, double bedZ, double wse, double saddleLipZ,
                                                   bool isOvertopping, double filledPct, double totalPondedMCM) {
    if (lblHydroTime) {
        lblHydroTime->setText(QString("T + %1:00 (%2 min)").arg(minute, 2, 10, QChar('0')).arg(minute));
    }
    if (lblHydroBasin) {
        lblHydroBasin->setText(basinName.isEmpty() ? "Basin 1: Gorge Foot Depression" : basinName);
    }
    if (lblHydroElev) {
        lblHydroElev->setText(QString("Bed: %1m | Saddle Lip: %2m MSL").arg(bedZ, 0, 'f', 1).arg(saddleLipZ, 0, 'f', 1));
    }
    if (lblHydroWSE) {
        lblHydroWSE->setText(QString("%1 m MSL (Depth: %2m)").arg(wse, 0, 'f', 2).arg(maxDepthM, 0, 'f', 2));
    }
    if (lblHydroSpillStatus) {
        if (minute == 0) {
            lblHydroSpillStatus->setText("Quiescent (Pre-Breach)");
        } else if (isOvertopping) {
            double head = std::max(0.1, wse - saddleLipZ);
            lblHydroSpillStatus->setText(QString("Saddle Overtopping (+%1m over lip)").arg(head, 0, 'f', 2));
        } else {
            lblHydroSpillStatus->setText(QString("Filling Depression (%1% capacity)").arg(filledPct, 0, 'f', 0));
        }
    }
    if (lblHydroPondVol) {
        lblHydroPondVol->setText(QString("%1 MCM stored in basins").arg(totalPondedMCM, 0, 'f', 1));
    }
    if (lblHydroArea) {
        lblHydroArea->setText(QString("%1 km² (%2 ha)").arg(areaKm2, 0, 'f', 2).arg(areaKm2 * 100.0, 0, 'f', 0));
    }
    if (lblHydroFront) {
        lblHydroFront->setText(QString("%1 km downstream").arg(frontDistKm, 0, 'f', 2));
    }
    if (lblHydroDepth) {
        lblHydroDepth->setText(QString("%1 m").arg(maxDepthM, 0, 'f', 2));
    }
    if (lblHydroVel) {
        double velKmh = maxVelMs * 3.6;
        lblHydroVel->setText(QString("%1 m/s (%2 km/h)").arg(maxVelMs, 0, 'f', 2).arg(velKmh, 0, 'f', 1));
    }
    if (lblHydroDischarge) {
        lblHydroDischarge->setText(QString("%1 m³/s").arg(peakDischargeQ, 0, 'f', 0));
    }
}

} // namespace MapUI
