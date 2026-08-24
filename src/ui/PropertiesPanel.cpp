#include "PropertiesPanel.h"
#include "IconHelper.h"
#include "../core/DamManager.h"
#include <QSplitter>

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
        QWidget#iconStrip, QWidget#topIconStrip, QWidget#bottomIconStrip {
            background-color: #121214;
            border-right: 1px solid #27272A;
        }
        QWidget#iconStrip QPushButton, QWidget#topIconStrip QPushButton, QWidget#bottomIconStrip QPushButton {
            background-color: transparent;
            border: none;
            border-left: 3px solid transparent;
            border-radius: 0px;
            min-width: 40px;
            max-width: 40px;
            min-height: 40px;
            max-height: 40px;
        }

        /* 1. Fluid & Hydrodynamics Tab (Sky Blue Theme) */
        QPushButton#btnTabFluid:hover {
            background-color: rgba(56, 189, 248, 0.10);
        }
        QPushButton#btnTabFluid:checked {
            background-color: rgba(56, 189, 248, 0.18);
            border-left: 3px solid #38BDF8;
        }

        /* 2. Terrain & Embankments Tab (Emerald Theme) */
        QPushButton#btnTabTerrain:hover {
            background-color: rgba(52, 211, 153, 0.10);
        }
        QPushButton#btnTabTerrain:checked {
            background-color: rgba(52, 211, 153, 0.18);
            border-left: 3px solid #34D399;
        }

        /* 3. Telemetry Tab (Amber Theme) */
        QPushButton#btnTabTelemetry:hover {
            background-color: rgba(253, 214, 99, 0.10);
        }
        QPushButton#btnTabTelemetry:checked {
            background-color: rgba(253, 214, 99, 0.18);
            border-left: 3px solid #FDD663;
        }

        /* 4. Display / Overlays Tab (Purple Theme) */
        QPushButton#btnTabDisplay:hover {
            background-color: rgba(167, 139, 250, 0.10);
        }
        QPushButton#btnTabDisplay:checked {
            background-color: rgba(167, 139, 250, 0.18);
            border-left: 3px solid #A78BFA;
        }

        /* 5. Dam Specifications Tab (Red Theme) */
        QPushButton#btnTabDam:hover {
            background-color: rgba(239, 68, 68, 0.10);
        }
        QPushButton#btnTabDam:checked {
            background-color: rgba(239, 68, 68, 0.18);
            border-left: 3px solid #EF4444;
        }

        /* 6. Danger & Impact Zones Tab (Rose-Pink-Red Theme) */
        QPushButton#btnTabDanger:hover {
            background-color: rgba(244, 63, 94, 0.12);
        }
        QPushButton#btnTabDanger:checked {
            background-color: rgba(244, 63, 94, 0.20);
            border-left: 3px solid #F43F5E;
        }

        /* 7. Logs Tab (Gray Theme) */
        QPushButton#btnTabTerminal:hover {
            background-color: rgba(161, 161, 170, 0.10);
        }
        QPushButton#btnTabTerminal:checked {
            background-color: rgba(161, 161, 170, 0.18);
            border-left: 3px solid #A1A1AA;
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

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    sidebarSplitter = new QSplitter(Qt::Vertical, this);
    sidebarSplitter->setObjectName("sidebarSplitter");
    sidebarSplitter->setHandleWidth(2);
    sidebarSplitter->setChildrenCollapsible(false);
    sidebarSplitter->setStyleSheet(R"(
        QSplitter#sidebarSplitter::handle:vertical {
            background-color: #27272A;
            height: 2px;
        }
        QSplitter#sidebarSplitter::handle:vertical:hover {
            background-color: #38BDF8;
        }
    )");

    tabButtonGroup = new QButtonGroup(this);
    tabButtonGroup->setExclusive(true);

    // =========================================================================
    // 1. TOP PART (Top Tool Tabs Strip + Top Header & Stacked Property Pages)
    // =========================================================================
    auto* topWidget = new QWidget(sidebarSplitter);
    auto* topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);

    // 1.1 Top Vertical Icon Strip (Tabs 0-4)
    auto* topIconStrip = new QWidget(topWidget);
    topIconStrip->setObjectName("topIconStrip");
    topIconStrip->setFixedWidth(42);

    auto* topIsLayout = new QVBoxLayout(topIconStrip);
    topIsLayout->setContentsMargins(0, 4, 0, 4);
    topIsLayout->setSpacing(2);

    btnTabFluid = new QPushButton(topIconStrip);
    btnTabFluid->setObjectName("btnTabFluid");
    btnTabFluid->setIcon(IconHelper::rain(QColor(56, 189, 248), 22));
    btnTabFluid->setIconSize(QSize(22, 22));
    btnTabFluid->setCheckable(true);
    btnTabFluid->setToolTip("Fluid & Hydrodynamics");
    tabButtonGroup->addButton(btnTabFluid, 0);
    topIsLayout->addWidget(btnTabFluid);

    btnTabTerrain = new QPushButton(topIconStrip);
    btnTabTerrain->setObjectName("btnTabTerrain");
    btnTabTerrain->setIcon(IconHelper::sunFog(QColor(52, 211, 153), 22));
    btnTabTerrain->setIconSize(QSize(22, 22));
    btnTabTerrain->setCheckable(true);
    btnTabTerrain->setToolTip("Terrain & Embankments");
    tabButtonGroup->addButton(btnTabTerrain, 1);
    topIsLayout->addWidget(btnTabTerrain);

    btnTabTelemetry = new QPushButton(topIconStrip);
    btnTabTelemetry->setObjectName("btnTabTelemetry");
    btnTabTelemetry->setIcon(IconHelper::radar(QColor(253, 214, 99), 22));
    btnTabTelemetry->setIconSize(QSize(22, 22));
    btnTabTelemetry->setCheckable(true);
    btnTabTelemetry->setToolTip("Assam River Gauges Telemetry");
    tabButtonGroup->addButton(btnTabTelemetry, 2);
    topIsLayout->addWidget(btnTabTelemetry);

    btnTabDisplay = new QPushButton(topIconStrip);
    btnTabDisplay->setObjectName("btnTabDisplay");
    btnTabDisplay->setIcon(IconHelper::map(QColor(167, 139, 250), 22));
    btnTabDisplay->setIconSize(QSize(22, 22));
    btnTabDisplay->setCheckable(true);
    btnTabDisplay->setToolTip("Viewport Overlays & Heatmaps");
    tabButtonGroup->addButton(btnTabDisplay, 3);
    topIsLayout->addWidget(btnTabDisplay);

    btnTabDam = new QPushButton(topIconStrip);
    btnTabDam->setObjectName("btnTabDam");
    btnTabDam->setIcon(IconHelper::info(QColor(239, 68, 68), 22));
    btnTabDam->setIconSize(QSize(22, 22));
    btnTabDam->setCheckable(true);
    btnTabDam->setToolTip("Dam Specifications & Preferences");
    tabButtonGroup->addButton(btnTabDam, 4);
    topIsLayout->addWidget(btnTabDam);

    btnTabDanger = new QPushButton(topIconStrip);
    btnTabDanger->setObjectName("btnTabDanger");
    btnTabDanger->setIcon(IconHelper::danger(QColor(244, 63, 94), 22));
    btnTabDanger->setIconSize(QSize(22, 22));
    btnTabDanger->setCheckable(true);
    btnTabDanger->setToolTip("Danger & Vulnerable Impact Zones");
    tabButtonGroup->addButton(btnTabDanger, 5);
    topIsLayout->addWidget(btnTabDanger);

    topIsLayout->addStretch(1);
    topLayout->addWidget(topIconStrip);

    // 1.2 Top Content Area (Header + Stacked Pages)
    auto* topContentArea = new QWidget(topWidget);
    auto* tcaLayout = new QVBoxLayout(topContentArea);
    tcaLayout->setContentsMargins(0, 0, 0, 0);
    tcaLayout->setSpacing(0);

    // Header Strip
    auto* topHeader = new QWidget(topContentArea);
    topHeader->setStyleSheet("background-color: #242424; border-bottom: 1px solid #1D1D1D;");
    auto* thLayout = new QHBoxLayout(topHeader);
    thLayout->setContentsMargins(12, 8, 12, 8);

    lblPanelTitle = new QLabel("Fluid & Hydrodynamics", topHeader);
    lblPanelTitle->setStyleSheet("color: #FFFFFF; font-weight: bold; font-size: 12px;");
    thLayout->addWidget(lblPanelTitle);
    thLayout->addStretch();

    tcaLayout->addWidget(topHeader);

    // Page Stack
    pageStack = new QStackedWidget(topContentArea);
    pageStack->addWidget(createFluidTab());
    pageStack->addWidget(createTerrainTab());
    pageStack->addWidget(createTelemetryTab());
    pageStack->addWidget(createDisplayTab());
    pageStack->addWidget(createDamTab());
    pageStack->addWidget(createDangerTab());

    tcaLayout->addWidget(pageStack, 1);
    topLayout->addWidget(topContentArea, 1);
    sidebarSplitter->addWidget(topWidget);

    // =========================================================================
    // 2. BOTTOM PART (Terminal Tab Icon Strip + Bottom xterm.js Terminal Console)
    // =========================================================================
    bottomWidget = new QWidget(sidebarSplitter);
    auto* bottomLayout = new QHBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);

    // 2.1 Bottom Icon Strip (Holds Cyan Terminal tab, matching terminal height)
    auto* bottomIconStrip = new QWidget(bottomWidget);
    bottomIconStrip->setObjectName("bottomIconStrip");
    bottomIconStrip->setFixedWidth(42);

    auto* bottomIsLayout = new QVBoxLayout(bottomIconStrip);
    bottomIsLayout->setContentsMargins(0, 2, 0, 2);
    bottomIsLayout->setSpacing(0);

    btnTabTerminal = new QPushButton(bottomIconStrip);
    btnTabTerminal->setObjectName("btnTabTerminal");
    btnTabTerminal->setIcon(IconHelper::terminal(QColor(161, 161, 170), 22));
    btnTabTerminal->setIconSize(QSize(22, 22));
    btnTabTerminal->setCheckable(true);
    btnTabTerminal->setChecked(false);
    btnTabTerminal->setToolTip("Logs");
    tabButtonGroup->addButton(btnTabTerminal, 6);
    bottomIsLayout->addWidget(btnTabTerminal);
    bottomIsLayout->addStretch(1);

    bottomLayout->addWidget(bottomIconStrip);

    // 2.2 Bottom Right Terminal Console
    terminalWidget = new SidebarTerminal(bottomWidget);
    bottomLayout->addWidget(terminalWidget, 1);

    sidebarSplitter->addWidget(bottomWidget);

    sidebarSplitter->setStretchFactor(0, 3);
    sidebarSplitter->setStretchFactor(1, 2);
    sidebarSplitter->setSizes({ 460, 230 });

    // Hidden by default
    bottomWidget->setVisible(false);

    mainLayout->addWidget(sidebarSplitter, 1);

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
    connect(btnTabDanger, &QPushButton::clicked, this, [this]() {
        switchTab(5, "Danger & Impact Assessment");
    });
    connect(btnTabTerminal, &QPushButton::clicked, this, [this]() {
        toggleTerminal();
    });

    btnTabFluid->setChecked(true);
    switchTab(0, "Fluid & Hydrodynamics");
}

void PropertiesPanel::toggleTerminal() {
    if (bottomWidget) {
        bool nextVis = !bottomWidget->isVisible();
        bottomWidget->setVisible(nextVis);
        if (btnTabTerminal) {
            btnTabTerminal->setChecked(nextVis);
        }
        if (nextVis && sidebarSplitter) {
            sidebarSplitter->setSizes({ std::max(100, height() - 230), 230 });
        }
    }
}

void PropertiesPanel::setTerminalVisible(bool visible) {
    if (bottomWidget) {
        bottomWidget->setVisible(visible);
        if (btnTabTerminal) {
            btnTabTerminal->setChecked(visible);
        }
        if (visible && sidebarSplitter) {
            sidebarSplitter->setSizes({ std::max(100, height() - 230), 230 });
        }
    }
}

bool PropertiesPanel::isTerminalVisible() const {
    return bottomWidget && bottomWidget->isVisible();
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

    if (terminalWidget) {
        terminalWidget->appendLog("DAM", QString("Selected asset: %1 (PIC: %2, River: %3)").arg(dam.name, dam.pic, dam.river), "#EF4444");
    }

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

    // Milestone logging to xterm.js terminal
    if (terminalWidget && (minute % 15 == 0 || isOvertopping)) {
        if (isOvertopping) {
            terminalWidget->appendLog("OVERTOP", QString("Saddle overtopping at %1 (WSE: %2m)").arg(basinName).arg(wse, 0, 'f', 1), "#EF4444");
        } else {
            terminalWidget->appendLog("HYDRO", QString("T+%1m: Front %2km · Area %3km² · Peak Q: %4 m³/s").arg(minute).arg(frontDistKm, 0, 'f', 1).arg(areaKm2, 0, 'f', 1).arg(peakDischargeQ, 0, 'f', 0), "#38BDF8");
        }
    }
}

QWidget* PropertiesPanel::createDangerTab() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 1. Danger Header Card (Same styling as Dam Header Card in Info tab)
    auto* headerCard = new QWidget(container);
    headerCard->setStyleSheet("background: transparent; border: none;");
    auto* hcLayout = new QVBoxLayout(headerCard);
    hcLayout->setContentsMargins(4, 4, 4, 4);
    hcLayout->setSpacing(4);

    lblDangerThreatLevel = new QLabel("Downstream Flood Hazard & Vulnerability", headerCard);
    lblDangerThreatLevel->setStyleSheet("color: #FFFFFF; font-size: 13px; font-weight: bold; background: transparent; border: none;");
    lblDangerThreatLevel->setWordWrap(true);

    lblDangerSubStatus = new QLabel("Impact Assessment: Standby (Ready for Simulation)", headerCard);
    lblDangerSubStatus->setStyleSheet("color: #FFFFFF; font-size: 11px; background: transparent; border: none;");

    lblDangerActionNotice = new QLabel("● Downstream Population & Infrastructure Risk", headerCard);
    lblDangerActionNotice->setStyleSheet("color: #FFFFFF; font-size: 11px; background: transparent; border: none;");

    hcLayout->addWidget(lblDangerThreatLevel);
    hcLayout->addWidget(lblDangerSubStatus);
    hcLayout->addWidget(lblDangerActionNotice);
    layout->addWidget(headerCard);

    // 2. Threat Overview & Impact Summary (Same format as Info tab sections)
    auto* secOverview = new CollapsibleSection("Threat Overview & Impact Summary", container);
    auto makeDataRow = [](const QString& labelText, QLabel*& valueLabel) {
        auto* row = new QWidget();
        auto* hl = new QHBoxLayout(row);
        hl->setContentsMargins(0, 2, 0, 2);
        auto* lbl = new QLabel(labelText, row);
        lbl->setStyleSheet("color: #D4D4D8; font-size: 11px; background: transparent; border: none;");
        lbl->setMinimumWidth(125);
        valueLabel = new QLabel("--", row);
        valueLabel->setStyleSheet("color: #FFFFFF; font-size: 11px; font-weight: normal; background: transparent; border: none;");
        valueLabel->setWordWrap(true);
        hl->addWidget(lbl);
        hl->addWidget(valueLabel, 1);
        return row;
    };

    secOverview->addWidget(makeDataRow("Threat Status:", lblDangerStatusText));
    secOverview->addWidget(makeDataRow("At-Risk Population:", lblDangerPopulationCount));
    secOverview->addWidget(makeDataRow("Inundated Zones:", lblDangerInundatedCount));
    secOverview->addWidget(makeDataRow("Next Wave ETA:", lblDangerFrontEta));
    secOverview->addWidget(makeDataRow("Downstream Reach:", lblDangerReachDist));
    layout->addWidget(secOverview);

    // 3. Downstream Vulnerable Areas & Infrastructure
    auto* secZones = new CollapsibleSection("Downstream Vulnerable Areas & Infrastructure", container);
    dangerCardsContainer = new QWidget(secZones);
    dangerCardsLayout = new QVBoxLayout(dangerCardsContainer);
    dangerCardsLayout->setContentsMargins(0, 0, 0, 0);
    dangerCardsLayout->setSpacing(8);

    auto* lblEmpty = new QLabel("No active flood simulation loaded.\nSelect a dam on the map to evaluate downstream danger zones.", dangerCardsContainer);
    lblEmpty->setObjectName("lblEmptyDanger");
    lblEmpty->setStyleSheet("color: #D4D4D8; font-size: 11px; background: transparent; border: none; padding: 12px;");
    lblEmpty->setAlignment(Qt::AlignCenter);
    dangerCardsLayout->addWidget(lblEmpty);

    secZones->addWidget(dangerCardsContainer);
    layout->addWidget(secZones);

    layout->addStretch();
    scroll->setWidget(container);
    return scroll;
}

void PropertiesPanel::updateDangerZones(const std::vector<MapCore::DangerZone>& zones, int currentMinute, double frontDistKm) {
    currentDangerZones = zones;
    if (!dangerCardsLayout) return;

    // Clear previous danger cards
    QLayoutItem* item;
    while ((item = dangerCardsLayout->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    if (zones.empty()) {
        auto* lblEmpty = new QLabel("No active flood simulation loaded.\nSelect a dam on the map to evaluate downstream danger zones.", dangerCardsContainer);
        lblEmpty->setStyleSheet("color: #D4D4D8; font-size: 11px; background: transparent; border: none; padding: 12px;");
        lblEmpty->setAlignment(Qt::AlignCenter);
        dangerCardsLayout->addWidget(lblEmpty);

        if (lblDangerThreatLevel) lblDangerThreatLevel->setText("Downstream Flood Hazard & Vulnerability");
        if (lblDangerSubStatus) lblDangerSubStatus->setText("Impact Assessment: Standby (Ready for Simulation)");
        if (lblDangerStatusText) lblDangerStatusText->setText("Standby");
        if (lblDangerPopulationCount) lblDangerPopulationCount->setText("0 residents");
        if (lblDangerInundatedCount) lblDangerInundatedCount->setText("0 / 0 settlements");
        if (lblDangerFrontEta) lblDangerFrontEta->setText("T + 0 min (Standby)");
        if (lblDangerReachDist) lblDangerReachDist->setText("0.0 km");
        return;
    }

    int totalPop = 0;
    int inundatedCount = 0;
    double nextEta = -1.0;
    QString nextZoneName = "";

    for (const auto& z : zones) {
        totalPop += z.estimatedPopulation;
        bool isInundated = (frontDistKm >= z.distanceKm && currentMinute > 0);
        if (isInundated) {
            inundatedCount++;
        } else if (nextEta < 0 && z.arrivalTimeMin >= currentMinute) {
            nextEta = z.arrivalTimeMin;
            nextZoneName = z.name.section('-', -1).trimmed();
        }

        // Card Container (Same card theme as Info tab)
        auto* card = new QWidget(dangerCardsContainer);
        card->setStyleSheet(QString(R"(
            QWidget {
                background-color: #1E1E22;
                border: 1px solid %1;
                border-radius: 4px;
            }
        )").arg(isInundated ? "#EF4444" : "#27272A"));

        auto* cLayout = new QVBoxLayout(card);
        cLayout->setContentsMargins(8, 8, 8, 8);
        cLayout->setSpacing(4);

        // Header: Name + Danger Level Badge
        auto* hRow = new QHBoxLayout();
        hRow->setContentsMargins(0, 0, 0, 0);

        auto* lblName = new QLabel(z.name, card);
        lblName->setStyleSheet("color: #FFFFFF; font-size: 12px; font-weight: bold; background: transparent; border: none;");
        lblName->setWordWrap(true);
        hRow->addWidget(lblName, 1);

        auto* lblBadge = new QLabel(z.riskLevel, card);
        lblBadge->setFixedHeight(16);
        lblBadge->setAlignment(Qt::AlignCenter);

        if (z.riskLevel == "CRITICAL") {
            lblBadge->setStyleSheet(R"(
                QLabel {
                    color: #FB7185;
                    background-color: rgba(244, 63, 94, 0.22);
                    border: 1px solid rgba(244, 63, 94, 0.45);
                    border-radius: 8px;
                    padding: 0px 6px;
                    min-height: 16px;
                    max-height: 16px;
                    font-family: 'Segoe UI', Inter, -apple-system, sans-serif;
                    font-size: 9px;
                    font-weight: 700;
                }
            )");
        } else if (z.riskLevel == "HIGH") {
            lblBadge->setStyleSheet(R"(
                QLabel {
                    color: #FB923C;
                    background-color: rgba(234, 88, 12, 0.22);
                    border: 1px solid rgba(251, 146, 60, 0.45);
                    border-radius: 8px;
                    padding: 0px 6px;
                    min-height: 16px;
                    max-height: 16px;
                    font-family: 'Segoe UI', Inter, -apple-system, sans-serif;
                    font-size: 9px;
                    font-weight: 700;
                }
            )");
        } else if (z.riskLevel == "MODERATE") {
            lblBadge->setStyleSheet(R"(
                QLabel {
                    color: #FCD34D;
                    background-color: rgba(217, 119, 6, 0.22);
                    border: 1px solid rgba(251, 191, 36, 0.45);
                    border-radius: 8px;
                    padding: 0px 6px;
                    min-height: 16px;
                    max-height: 16px;
                    font-family: 'Segoe UI', Inter, -apple-system, sans-serif;
                    font-size: 9px;
                    font-weight: 700;
                }
            )");
        } else {
            lblBadge->setStyleSheet(R"(
                QLabel {
                    color: #38BDF8;
                    background-color: rgba(2, 132, 199, 0.22);
                    border: 1px solid rgba(56, 189, 248, 0.40);
                    border-radius: 8px;
                    padding: 0px 6px;
                    min-height: 16px;
                    max-height: 16px;
                    font-family: 'Segoe UI', Inter, -apple-system, sans-serif;
                    font-size: 9px;
                    font-weight: 700;
                }
            )");
        }
        hRow->addWidget(lblBadge, 0);
        cLayout->addLayout(hRow);

        // Dynamic Inundation Status Line (Matching Info tab typography)
        auto* lblStatus = new QLabel(card);
        if (isInundated) {
            double curDepth = std::max(0.5, z.peakDepthM * std::min(1.0, 1.0 - 0.01 * (currentMinute - z.arrivalTimeMin)));
            lblStatus->setText(QString("🔴 Inundated (Current Depth: %1m)").arg(curDepth, 0, 'f', 1));
            lblStatus->setStyleSheet("color: #EF4444; font-size: 11px; font-weight: bold; background: transparent; border: none;");
        } else if (currentMinute > 0 && currentMinute < z.arrivalTimeMin) {
            int remMin = static_cast<int>(std::round(z.arrivalTimeMin - currentMinute));
            lblStatus->setText(QString("⚠️ Impending Wave · ETA: T + %1m (In %2 min)").arg(qRound(z.arrivalTimeMin)).arg(remMin));
            lblStatus->setStyleSheet("color: #FFFFFF; font-size: 11px; font-weight: bold; background: transparent; border: none;");
        } else {
            lblStatus->setText(QString("⏳ Projected Arrival ETA: T + %1 min").arg(qRound(z.arrivalTimeMin)));
            lblStatus->setStyleSheet("color: #D4D4D8; font-size: 11px; background: transparent; border: none;");
        }
        cLayout->addWidget(lblStatus);

        // Details Rows (Formatted with exact same color tokens as createDamTab in Info tab)
        auto addCardRow = [&](const QString& labelText, const QString& valText, const QString& valColor = "#FFFFFF") {
            auto* row = new QWidget(card);
            row->setStyleSheet("background: transparent; border: none;");
            auto* hl = new QHBoxLayout(row);
            hl->setContentsMargins(0, 1, 0, 1);
            auto* lbl = new QLabel(labelText, row);
            lbl->setStyleSheet("color: #D4D4D8; font-size: 11px; background: transparent; border: none;");
            lbl->setMinimumWidth(115);
            auto* val = new QLabel(valText, row);
            val->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: normal; background: transparent; border: none;").arg(valColor));
            val->setWordWrap(true);
            hl->addWidget(lbl);
            hl->addWidget(val, 1);
            cLayout->addWidget(row);
        };

        addCardRow("Zone Type:", z.zoneType);
        addCardRow("Coordinates:", QString("%1° N, %2° E").arg(z.lat, 0, 'f', 4).arg(z.lon, 0, 'f', 4));
        addCardRow("Ground Elevation:", QString("%1 m MSL").arg(z.elevationMSL, 0, 'f', 1));
        addCardRow("Breach Distance:", QString("%1 km downstream").arg(z.distanceKm, 0, 'f', 1));
        addCardRow("Peak Inundation:", QString("%1 m depth (Flow Vel: %2 m/s)").arg(z.peakDepthM, 0, 'f', 1).arg(z.peakVelocityMs, 0, 'f', 1));
        addCardRow("At-Risk Population:", QString("%1 residents").arg(QLocale(QLocale::English).toString(z.estimatedPopulation)));
        addCardRow("Critical Assets:", z.criticalInfrastructure);
        addCardRow("Action Advice:", z.evacuationAdvice, isInundated ? "#EF4444" : "#FFFFFF");

        // Locate Button (Same button styling as btnCenterOnDam in Info tab)
        auto* btnLocate = new QPushButton("🎯 Locate Zone on Map", card);
        btnLocate->setObjectName("btnActionBlue");
        double zLat = z.lat;
        double zLon = z.lon;
        connect(btnLocate, &QPushButton::clicked, this, [this, zLat, zLon]() {
            emit centerLocationRequested(zLat, zLon, 13);
        });
        cLayout->addWidget(btnLocate);

        dangerCardsLayout->addWidget(card);
    }

    // Update Overview Header & Stats
    if (lblDangerThreatLevel) {
        if (inundatedCount > 0) {
            lblDangerThreatLevel->setText("CRITICAL FLOOD INUNDATION ACTIVE");
        } else if (currentMinute > 0) {
            lblDangerThreatLevel->setText("FLOOD WAVE ADVANCING DOWNSTREAM");
        } else {
            lblDangerThreatLevel->setText("Downstream Flood Hazard & Vulnerability");
        }
    }

    if (lblDangerSubStatus) {
        if (inundatedCount > 0) {
            lblDangerSubStatus->setText(QString("Inundation Impact: %1 of %2 Downstream Zones Breached").arg(inundatedCount).arg(zones.size()));
        } else if (currentMinute > 0) {
            lblDangerSubStatus->setText(QString("Active Propagation: T + %1 min · Front %2 km").arg(currentMinute).arg(frontDistKm, 0, 'f', 1));
        } else {
            lblDangerSubStatus->setText("Vulnerability Level: High Risk Assessment");
        }
    }

    if (lblDangerStatusText) {
        if (inundatedCount > 0) {
            lblDangerStatusText->setText(QString("%1 Zones Inundated").arg(inundatedCount));
            lblDangerStatusText->setStyleSheet("color: #EF4444; font-size: 11px; font-weight: bold; background: transparent; border: none;");
        } else if (currentMinute > 0) {
            lblDangerStatusText->setText("Wave Advancing");
            lblDangerStatusText->setStyleSheet("color: #FFFFFF; font-size: 11px; font-weight: bold; background: transparent; border: none;");
        } else {
            lblDangerStatusText->setText("Simulation Ready (T+0m)");
            lblDangerStatusText->setStyleSheet("color: #FFFFFF; font-size: 11px; font-weight: normal; background: transparent; border: none;");
        }
    }

    if (lblDangerPopulationCount) {
        lblDangerPopulationCount->setText(QString("%1 residents").arg(QLocale(QLocale::English).toString(totalPop)));
    }

    if (lblDangerInundatedCount) {
        lblDangerInundatedCount->setText(QString("%1 / %2 settlements").arg(inundatedCount).arg(zones.size()));
    }

    if (lblDangerFrontEta) {
        if (nextEta > 0) {
            int rem = static_cast<int>(std::round(nextEta - currentMinute));
            lblDangerFrontEta->setText(QString("%1 (In %2m)").arg(nextZoneName).arg(std::max(0, rem)));
        } else if (inundatedCount == static_cast<int>(zones.size())) {
            lblDangerFrontEta->setText("All downstream zones inundated");
        } else {
            lblDangerFrontEta->setText(QString("T + %1 min").arg(currentMinute));
        }
    }

    if (lblDangerReachDist) {
        lblDangerReachDist->setText(QString("%1 km (Current Front)").arg(frontDistKm, 0, 'f', 1));
    }
}

} // namespace MapUI
