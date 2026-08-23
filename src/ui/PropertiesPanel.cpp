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
    btnHeader->setIcon(IconHelper::chevronDown(QColor(200, 200, 205), 14));
    btnHeader->setIconSize(QSize(14, 14));
    btnHeader->setStyleSheet(R"(
        QPushButton {
            background-color: #303030;
            color: #E6E6E6;
            border: 1px solid #202020;
            border-top: 1px solid #3D3D3D;
            border-radius: 4px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: bold;
            text-align: left;
            padding: 5px 8px;
        }
        QPushButton:hover {
            background-color: #383838;
            color: #FFFFFF;
        }
    )");

    contentWidget = new QWidget(this);
    contentWidget->setStyleSheet(R"(
        QWidget {
            background-color: #242424;
            border: 1px solid #1E1E1E;
            border-top: none;
            border-bottom-left-radius: 4px;
            border-bottom-right-radius: 4px;
        }
    )");

    contentLayout = new QVBoxLayout(contentWidget);
    contentLayout->setContentsMargins(10, 8, 10, 8);
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
            background-color: #282828;
            border-left: 1px solid #181818;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
        }
        QWidget#iconStrip {
            background-color: #1E1E1E;
            border-right: 1px solid #151515;
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
            background-color: #2B2B2B;
        }
        QWidget#iconStrip QPushButton:checked {
            background-color: #2D323A;
            border-left: 3px solid #54D59A;
        }
        QScrollArea {
            border: none;
            background-color: #282828;
        }
        QScrollBar:vertical {
            background: #1E1E1E;
            width: 8px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3E3E3E;
            min-height: 20px;
            border-radius: 4px;
        }
        QScrollBar::handle:vertical:hover {
            background: #4E4E4E;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0px;
        }
        QLabel {
            color: #CCCCCC;
        }
        QDoubleSpinBox, QSlider {
            color: #FFFFFF;
        }
        QDoubleSpinBox {
            background-color: #181818;
            border: 1px solid #333333;
            border-radius: 4px;
            padding: 4px 6px;
            color: #FFFFFF;
            font-family: Consolas, monospace;
            font-size: 11px;
        }
        QDoubleSpinBox:focus {
            border-color: #54D59A;
        }
        QCheckBox {
            color: #CCCCCC;
            font-size: 11px;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            background-color: #181818;
            border: 1px solid #3A3A3A;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked {
            background-color: #54D59A;
            border-color: #54D59A;
        }
        QPushButton#btnBake {
            background-color: #E87D0D;
            color: #FFFFFF;
            border: 1px solid #FFA544;
            border-radius: 4px;
            font-size: 12px;
            font-weight: bold;
            padding: 8px 12px;
        }
        QPushButton#btnBake:hover {
            background-color: #FF9426;
        }
        QPushButton#btnBake:pressed {
            background-color: #C76505;
        }
        QPushButton#btnActionBlue {
            background-color: #2F4D7B;
            color: #FFFFFF;
            border: 1px solid #4772B3;
            border-radius: 4px;
            font-size: 11px;
            font-weight: bold;
            padding: 6px 10px;
        }
        QPushButton#btnActionBlue:hover {
            background-color: #3C639D;
        }
        QPushButton#btnReset {
            background-color: #303030;
            color: #CCCCCC;
            border: 1px solid #202020;
            border-radius: 4px;
            font-size: 11px;
            padding: 6px 12px;
        }
        QPushButton#btnReset:hover {
            background-color: #3C3C3C;
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

    auto makeGauge = [](const QString& name, const QString& level, const QString& status, const QString& color) {
        auto* card = new QWidget();
        card->setStyleSheet("background-color: #1E1E1E; border: 1px solid #333333; border-radius: 4px; padding: 4px;");
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(6, 4, 6, 4);
        cl->setSpacing(2);

        auto* top = new QHBoxLayout();
        auto* lblName = new QLabel(name, card);
        lblName->setStyleSheet("font-weight: bold; color: #FFFFFF; font-size: 11px;");
        auto* lblStatus = new QLabel(status, card);
        lblStatus->setStyleSheet(QString("font-weight: bold; color: %1; font-size: 10px;").arg(color));
        top->addWidget(lblName);
        top->addStretch();
        top->addWidget(lblStatus);

        auto* lblLevel = new QLabel("Current Level: " + level, card);
        lblLevel->setStyleSheet("color: #AAAAAA; font-size: 10px; font-family: Consolas, monospace;");

        cl->addLayout(top);
        cl->addWidget(lblLevel);
        return card;
    };

    secStations->addWidget(makeGauge("Guwahati (Brahmaputra)", "49.68 m", "ALERT (+0.68m)", "#E26D1E"));
    secStations->addWidget(makeGauge("Dibrugarh", "104.24 m", "NORMAL", "#81C995"));
    secStations->addWidget(makeGauge("Tezpur", "64.20 m", "NORMAL", "#81C995"));
    secStations->addWidget(makeGauge("Dhubri", "29.10 m", "WARNING", "#FDD663"));
    secStations->addWidget(makeGauge("Nematighat (Jorhat)", "85.90 m", "HIGH", "#E26D1E"));

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
    headerCard->setStyleSheet(R"(
        background-color: #1E1E22;
        border: 1px solid #32323A;
        border-left: 3px solid #54D59A;
        border-radius: 5px;
    )");
    auto* hcLayout = new QVBoxLayout(headerCard);
    hcLayout->setContentsMargins(10, 8, 10, 8);
    hcLayout->setSpacing(4);

    lblDamName = new QLabel("Select a Dam on Map", headerCard);
    lblDamName->setStyleSheet("color: #FFFFFF; font-size: 13px; font-weight: bold;");
    lblDamName->setWordWrap(true);

    lblDamPic = new QLabel("National PIC: --", headerCard);
    lblDamPic->setStyleSheet("color: #8AB4F8; font-family: Consolas, monospace; font-size: 10px; font-weight: bold;");

    lblDamStatus = new QLabel("● National Hydrological Asset", headerCard);
    lblDamStatus->setStyleSheet("color: #54D59A; font-size: 10px; font-weight: bold;");

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
        lbl->setStyleSheet("color: #9E9EA6; font-size: 10px;");
        lbl->setMinimumWidth(100);
        valueLabel = new QLabel("--", row);
        valueLabel->setStyleSheet("color: #E2E2E8; font-size: 11px; font-weight: 500;");
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

    progHydroTimeline = new QProgressBar(container);
    progHydroTimeline->setRange(0, 60);
    progHydroTimeline->setValue(0);
    progHydroTimeline->setTextVisible(true);
    progHydroTimeline->setFormat("T + %v min / 60 min");
    progHydroTimeline->setStyleSheet(R"(
        QProgressBar {
            background-color: #16161A;
            border: 1px solid #32323A;
            border-radius: 4px;
            height: 18px;
            text-align: center;
            color: #E2E2E8;
            font-size: 10px;
            font-weight: bold;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #1E88E5, stop:1 #00E5FF);
            border-radius: 3px;
        }
    )");
    secHydro->addWidget(progHydroTimeline);

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
        lbl->setStyleSheet("color: #9E9EA6; font-size: 10px;");
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
    if (progHydroTimeline) {
        progHydroTimeline->setValue(minute);
    }
    if (lblHydroTime) {
        lblHydroTime->setText(QString("T + %1:00 (%2 min)").arg(minute, 2, 10, QChar('0')).arg(minute));
        lblHydroTime->setStyleSheet("color: #00E5FF; font-weight: bold; font-family: Consolas, monospace;");
    }
    if (lblHydroBasin) {
        lblHydroBasin->setText(basinName.isEmpty() ? "Basin 1: Gorge Foot Depression" : basinName);
        lblHydroBasin->setStyleSheet("color: #E2E2E8; font-weight: bold;");
    }
    if (lblHydroElev) {
        lblHydroElev->setText(QString("Bed: %1m | Saddle Lip: %2m MSL").arg(bedZ, 0, 'f', 1).arg(saddleLipZ, 0, 'f', 1));
        lblHydroElev->setStyleSheet("color: #9E9EA6; font-size: 10px; font-weight: bold;");
    }
    if (lblHydroWSE) {
        lblHydroWSE->setText(QString("%1 m MSL (Depth: %2m)").arg(wse, 0, 'f', 2).arg(maxDepthM, 0, 'f', 2));
        lblHydroWSE->setStyleSheet("color: #54D59A; font-weight: bold;");
    }
    if (lblHydroSpillStatus) {
        if (minute == 0) {
            lblHydroSpillStatus->setText("⚪ Quiescent (Pre-Breach)");
            lblHydroSpillStatus->setStyleSheet("color: #9E9EA6; font-weight: bold;");
        } else if (isOvertopping) {
            double head = std::max(0.1, wse - saddleLipZ);
            lblHydroSpillStatus->setText(QString("⚡ SADDLE OVERTOPPING (+%1m over lip)").arg(head, 0, 'f', 2));
            lblHydroSpillStatus->setStyleSheet("color: #00E5FF; font-weight: bold;");
        } else {
            lblHydroSpillStatus->setText(QString("⏳ Filling Depression (%1% capacity)").arg(filledPct, 0, 'f', 0));
            lblHydroSpillStatus->setStyleSheet("color: #FDD663; font-weight: bold;");
        }
    }
    if (lblHydroPondVol) {
        lblHydroPondVol->setText(QString("%1 MCM stored in basins").arg(totalPondedMCM, 0, 'f', 1));
        lblHydroPondVol->setStyleSheet("color: #8AB4F8; font-weight: bold;");
    }
    if (lblHydroArea) {
        lblHydroArea->setText(QString("%1 km² (%2 ha)").arg(areaKm2, 0, 'f', 2).arg(areaKm2 * 100.0, 0, 'f', 0));
        lblHydroArea->setStyleSheet("color: #8AB4F8; font-weight: bold;");
    }
    if (lblHydroFront) {
        lblHydroFront->setText(QString("%1 km downstream").arg(frontDistKm, 0, 'f', 2));
        lblHydroFront->setStyleSheet("color: #FDD663; font-weight: bold;");
    }
    if (lblHydroDepth) {
        lblHydroDepth->setText(QString("%1 m").arg(maxDepthM, 0, 'f', 2));
        lblHydroDepth->setStyleSheet("color: #54D59A; font-weight: bold;");
    }
    if (lblHydroVel) {
        double velKmh = maxVelMs * 3.6;
        lblHydroVel->setText(QString("%1 m/s (%2 km/h)").arg(maxVelMs, 0, 'f', 2).arg(velKmh, 0, 'f', 1));
        lblHydroVel->setStyleSheet("color: #FFA544; font-weight: bold;");
    }
    if (lblHydroDischarge) {
        lblHydroDischarge->setText(QString("%1 m³/s").arg(peakDischargeQ, 0, 'f', 0));
        lblHydroDischarge->setStyleSheet("color: #E2E2E8; font-weight: bold;");
    }
}

} // namespace MapUI
