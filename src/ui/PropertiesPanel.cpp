#include "PropertiesPanel.h"

namespace MapUI {

// ==========================================
// CollapsibleSection Implementation
// ==========================================

CollapsibleSection::CollapsibleSection(const QString& title, QWidget* parent) : QWidget(parent) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 2, 0, 4);
    mainLayout->setSpacing(0);

    // Section Header Button
    btnHeader = new QPushButton("▼ " + title, this);
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

    QString text = btnHeader->text();
    if (text.startsWith("▼ ")) {
        btnHeader->setText("▶ " + text.mid(2));
    } else if (text.startsWith("▶ ")) {
        btnHeader->setText("▼ " + text.mid(2));
    }
}

// ==========================================
// PropertiesPanel Implementation
// ==========================================

PropertiesPanel::PropertiesPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void PropertiesPanel::setupUi() {
    setMinimumWidth(280);

    setStyleSheet(R"(
        QWidget#propertiesPanel {
            background-color: #282828;
            border-left: 1px solid #181818;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
        }
        QTabWidget::pane {
            border: none;
            background-color: #282828;
        }
        QTabBar::tab {
            background-color: #242424;
            color: #999999;
            border: 1px solid #1E1E1E;
            border-bottom: none;
            padding: 6px 12px;
            font-size: 11px;
            font-weight: bold;
            min-width: 50px;
        }
        QTabBar::tab:hover {
            background-color: #2E2E2E;
            color: #CCCCCC;
        }
        QTabBar::tab:selected {
            background-color: #282828;
            color: #FFFFFF;
            border-top: 2px solid #E87D0D;
        }
        QScrollArea {
            border: none;
            background-color: transparent;
        }
        QLabel {
            color: #CCCCCC;
            font-size: 11px;
        }
        QDoubleSpinBox {
            background-color: #181818;
            color: #E6E6E6;
            border: 1px solid #333333;
            border-radius: 3px;
            font-family: Consolas, 'Segoe UI', monospace;
            font-size: 11px;
            font-weight: bold;
            padding: 2px 6px;
            min-height: 22px;
        }
        QDoubleSpinBox:hover {
            border-color: #4772B3;
        }
        QDoubleSpinBox:focus {
            border-color: #5680C2;
            background-color: #111111;
        }
        QCheckBox {
            color: #CCCCCC;
            font-size: 11px;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border-radius: 3px;
            border: 1px solid #444444;
            background-color: #181818;
        }
        QCheckBox::indicator:checked {
            background-color: #4772B3;
            border-color: #5680C2;
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

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Header Strip
    auto* topHeader = new QWidget(this);
    topHeader->setStyleSheet("background-color: #242424; border-bottom: 1px solid #1D1D1D;");
    auto* thLayout = new QHBoxLayout(topHeader);
    thLayout->setContentsMargins(10, 6, 10, 6);

    auto* lblPanelTitle = new QLabel("Properties & Physics", topHeader);
    lblPanelTitle->setStyleSheet("color: #FFFFFF; font-weight: bold; font-size: 12px;");
    thLayout->addWidget(lblPanelTitle);
    thLayout->addStretch();

    rootLayout->addWidget(topHeader);

    // Tab Widget
    tabWidget = new QTabWidget(this);
    tabWidget->addTab(createFluidTab(), "🌊 Fluid");
    tabWidget->addTab(createTerrainTab(), "🏔️ Terrain");
    tabWidget->addTab(createTelemetryTab(), "📊 Gauges");
    tabWidget->addTab(createDisplayTab(), "👁️ View");

    rootLayout->addWidget(tabWidget, 1);
}

QWidget* PropertiesPanel::createFluidTab() {
    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);

    auto* container = new QWidget();
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // Section 1: Inflow & Boundary Conditions
    auto* secBoundary = new CollapsibleSection("Flood Boundary Dynamics", container);

    auto makeRow = [](const QString& labelText, QWidget* control) {
        auto* row = new QWidget();
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 0, 0, 0);
        auto* lbl = new QLabel(labelText, row);
        lbl->setMinimumWidth(110);
        rl->addWidget(lbl);
        rl->addWidget(control, 1);
        return row;
    };

    spinWaterRise = new QDoubleSpinBox();
    spinWaterRise->setRange(0.0, 20.0);
    spinWaterRise->setValue(2.40);
    spinWaterRise->setSingleStep(0.1);
    spinWaterRise->setSuffix(" m");
    secBoundary->addWidget(makeRow("Peak Water Rise:", spinWaterRise));

    spinRainfall = new QDoubleSpinBox();
    spinRainfall->setRange(0.0, 500.0);
    spinRainfall->setValue(85.0);
    spinRainfall->setSingleStep(5.0);
    spinRainfall->setSuffix(" mm/h");
    secBoundary->addWidget(makeRow("Rainfall Rate:", spinRainfall));

    spinBreachWidth = new QDoubleSpinBox();
    spinBreachWidth->setRange(0.0, 1000.0);
    spinBreachWidth->setValue(120.0);
    spinBreachWidth->setSingleStep(10.0);
    spinBreachWidth->setSuffix(" m");
    secBoundary->addWidget(makeRow("Breach Width:", spinBreachWidth));

    layout->addWidget(secBoundary);

    // Section 2: Fluid Physics
    auto* secPhysics = new CollapsibleSection("Fluid Physics & Roughness", container);

    spinVelocity = new QDoubleSpinBox();
    spinVelocity->setRange(0.1, 15.0);
    spinVelocity->setValue(2.1);
    spinVelocity->setSingleStep(0.2);
    spinVelocity->setSuffix(" m/s");
    secPhysics->addWidget(makeRow("Flow Velocity:", spinVelocity));

    auto* spinManning = new QDoubleSpinBox();
    spinManning->setRange(0.01, 0.15);
    spinManning->setValue(0.035);
    spinManning->setSingleStep(0.005);
    spinManning->setDecimals(3);
    secPhysics->addWidget(makeRow("Manning (n):", spinManning));

    layout->addWidget(secPhysics);

    // Section 3: Bake & Execution
    auto* secBake = new CollapsibleSection("Bake Simulation", container);

    btnBakeSim = new QPushButton("🌊 Bake Simulation (Assam)", container);
    btnBakeSim->setObjectName("btnBake");
    secBake->addWidget(btnBakeSim);

    btnResetSim = new QPushButton("🔄 Clear Cache & Reset", container);
    btnResetSim->setObjectName("btnReset");
    secBake->addWidget(btnResetSim);

    layout->addWidget(secBake);
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

    auto* secDEM = new CollapsibleSection("Digital Elevation Model", container);
    auto* chkContours = new QCheckBox("Elevation Contour Lines (10m)", container);
    chkContours->setChecked(true);
    auto* chkHillshade = new QCheckBox("3D Hillshade & Shading", container);
    chkHillshade->setChecked(true);
    auto* chkSlope = new QCheckBox("Slope Steepness Overlay", container);

    secDEM->addWidget(chkContours);
    secDEM->addWidget(chkHillshade);
    secDEM->addWidget(chkSlope);
    layout->addWidget(secDEM);

    auto* secEmbankments = new CollapsibleSection("Embankments & Dykes", container);
    auto* chkDykes = new QCheckBox("Brahmaputra Embankment System", container);
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

    secStations->addWidget(makeGauge("📍 Guwahati (Brahmaputra)", "49.68 m", "▲ ALERT (+0.68m)", "#E26D1E"));
    secStations->addWidget(makeGauge("📍 Dibrugarh", "104.24 m", "● NORMAL", "#81C995"));
    secStations->addWidget(makeGauge("📍 Tezpur", "64.20 m", "● NORMAL", "#81C995"));
    secStations->addWidget(makeGauge("📍 Dhubri", "29.10 m", "▲ WARNING", "#FDD663"));
    secStations->addWidget(makeGauge("📍 Nematighat (Jorhat)", "85.90 m", "▲ HIGH", "#E26D1E"));

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

} // namespace MapUI
