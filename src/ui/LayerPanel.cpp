#include "LayerPanel.h"
#include <QGraphicsDropShadowEffect>

namespace MapUI {

LayerPanel::LayerPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void LayerPanel::setupUi() {
    setFixedWidth(220);
    setStyleSheet(R"(
        QWidget#layerPanelContainer {
            background-color: #202124;
            border: 1px solid #3C4043;
            border-radius: 10px;
        }
        QLabel {
            font-family: 'Segoe UI', Arial, sans-serif;
            color: #E8EAED;
        }
        QComboBox {
            background-color: #303134;
            border: 1px solid #3C4043;
            border-radius: 6px;
            padding: 5px 10px;
            font-size: 12px;
            color: #E8EAED;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #202124;
            border: 1px solid #3C4043;
            color: #E8EAED;
            selection-background-color: #38465C;
            selection-color: #8AB4F8;
        }
        QCheckBox {
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 12px;
            color: #E8EAED;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 4px;
            border: 1px solid #5F6368;
            background-color: #202124;
        }
        QCheckBox::indicator:checked {
            background-color: #8AB4F8;
            border-color: #8AB4F8;
        }
    )");

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setColor(QColor(0, 0, 0, 90));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* container = new QWidget(this);
    container->setObjectName("layerPanelContainer");
    rootLayout->addWidget(container);

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    auto* lblHeader = new QLabel("🗺️ Map Layers & Style", container);
    lblHeader->setStyleSheet("font-weight: bold; font-size: 13px; color: #8AB4F8;");

    comboTheme = new QComboBox(container);
    comboTheme->addItem("🌙 Google Dark", static_cast<int>(MapRenderer::ThemePreset::GOOGLE_DARK));
    comboTheme->addItem("🗺️ Google Light", static_cast<int>(MapRenderer::ThemePreset::GOOGLE_LIGHT));
    comboTheme->addItem("🌲 Terrain Nature", static_cast<int>(MapRenderer::ThemePreset::TERRAIN_NATURE));
    comboTheme->addItem("⚡ High Contrast", static_cast<int>(MapRenderer::ThemePreset::HIGH_CONTRAST));

    checkRoads = new QCheckBox("🛣️ Roads & Highways", container);
    checkRoads->setChecked(true);

    checkWater = new QCheckBox("🌊 Rivers & Water", container);
    checkWater->setChecked(true);

    checkLanduse = new QCheckBox("🌳 Forests & Parks", container);
    checkLanduse->setChecked(true);

    checkBuildings = new QCheckBox("🏢 3D Buildings", container);
    checkBuildings->setChecked(true);

    checkLabels = new QCheckBox("🏷️ City & Town Labels", container);
    checkLabels->setChecked(true);

    checkPois = new QCheckBox("🏥 POIs & Amenities", container);
    checkPois->setChecked(true);

    layout->addWidget(lblHeader);
    layout->addSpacing(2);
    layout->addWidget(comboTheme);
    layout->addSpacing(4);
    layout->addWidget(checkRoads);
    layout->addWidget(checkWater);
    layout->addWidget(checkLanduse);
    layout->addWidget(checkBuildings);
    layout->addWidget(checkLabels);
    layout->addWidget(checkPois);

    auto onOptionToggled = [this]() {
        currentOptions.showRoads = checkRoads->isChecked();
        currentOptions.showWater = checkWater->isChecked();
        currentOptions.showLanduse = checkLanduse->isChecked();
        currentOptions.showBuildings = checkBuildings->isChecked();
        currentOptions.showLabels = checkLabels->isChecked();
        currentOptions.showPois = checkPois->isChecked();
        emit optionsChanged(currentOptions);
    };

    connect(comboTheme, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        auto preset = static_cast<MapRenderer::ThemePreset>(comboTheme->itemData(idx).toInt());
        emit themeChanged(preset);
    });

    connect(checkRoads, &QCheckBox::toggled, this, onOptionToggled);
    connect(checkWater, &QCheckBox::toggled, this, onOptionToggled);
    connect(checkLanduse, &QCheckBox::toggled, this, onOptionToggled);
    connect(checkBuildings, &QCheckBox::toggled, this, onOptionToggled);
    connect(checkLabels, &QCheckBox::toggled, this, onOptionToggled);
    connect(checkPois, &QCheckBox::toggled, this, onOptionToggled);
}

void LayerPanel::setOptions(const MapRenderer::RenderOptions& opt) {
    currentOptions = opt;
    checkRoads->setChecked(opt.showRoads);
    checkWater->setChecked(opt.showWater);
    checkLanduse->setChecked(opt.showLanduse);
    checkBuildings->setChecked(opt.showBuildings);
    checkLabels->setChecked(opt.showLabels);
    checkPois->setChecked(opt.showPois);
}

} // namespace MapUI
