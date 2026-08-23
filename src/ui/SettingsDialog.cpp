#include "SettingsDialog.h"
#include <QGraphicsDropShadowEffect>

namespace MapUI {

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Settings & Preferences");
    setModal(true);
    setFixedSize(480, 520);

    setupUi();
    loadSettings();
}

void SettingsDialog::setupUi() {
    setStyleSheet(R"(
        QDialog {
            background-color: #18181B;
            color: #FAFAFA;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
        }
        QGroupBox {
            background-color: #202124;
            border: 1px solid #3C4043;
            border-radius: 8px;
            margin-top: 14px;
            padding: 12px 14px;
            font-weight: bold;
            font-size: 13px;
            color: #8AB4F8;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 6px;
            color: #8AB4F8;
        }
        QLabel {
            color: #E8EAED;
            font-size: 12px;
        }
        QSlider::groove:horizontal {
            height: 6px;
            background: #3C4043;
            border-radius: 3px;
        }
        QSlider::sub-page:horizontal {
            background: #8AB4F8;
            border-radius: 3px;
        }
        QSlider::handle:horizontal {
            background: #FFFFFF;
            border: 2px solid #8AB4F8;
            width: 16px;
            margin-top: -6px;
            margin-bottom: -6px;
            border-radius: 8px;
        }
        QSlider::handle:horizontal:hover {
            background: #8AB4F8;
            border-color: #FFFFFF;
        }
        QRadioButton {
            color: #FAFAFA;
            font-size: 12px;
            spacing: 6px;
        }
        QRadioButton::indicator {
            width: 14px;
            height: 14px;
        }
        QRadioButton::indicator:checked {
            background-color: #8AB4F8;
            border: 2px solid #FFFFFF;
            border-radius: 7px;
        }
        QRadioButton::indicator:unchecked {
            background-color: #27272A;
            border: 1px solid #71717A;
            border-radius: 7px;
        }
        QCheckBox {
            color: #FAFAFA;
            font-size: 12px;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 14px;
            height: 14px;
            border-radius: 3px;
            border: 1px solid #71717A;
            background-color: #27272A;
        }
        QCheckBox::indicator:checked {
            background-color: #8AB4F8;
            border-color: #8AB4F8;
        }
        QComboBox {
            background-color: #27272A;
            color: #FAFAFA;
            border: 1px solid #3C4043;
            border-radius: 6px;
            padding: 4px 10px;
            font-size: 12px;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox QAbstractItemView {
            background-color: #202124;
            color: #FAFAFA;
            selection-background-color: #3C4043;
            selection-color: #8AB4F8;
            border: 1px solid #3C4043;
        }
        QPushButton {
            background-color: #27272A;
            color: #FAFAFA;
            border: 1px solid #3C4043;
            border-radius: 6px;
            font-weight: bold;
            font-size: 12px;
            padding: 8px 18px;
        }
        QPushButton:hover {
            background-color: #3C4043;
            color: #8AB4F8;
            border-color: #8AB4F8;
        }
        QPushButton#btnSave {
            background-color: #8AB4F8;
            color: #18181B;
            border-color: #8AB4F8;
        }
        QPushButton#btnSave:hover {
            background-color: #A8C7FA;
        }
    )");

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(18, 16, 18, 16);
    mainLayout->setSpacing(12);

    // Dialog Header
    auto* headerLabel = new QLabel("⚙️ Application Settings", this);
    headerLabel->setStyleSheet("font-size: 17px; font-weight: bold; color: #FFFFFF;");
    mainLayout->addWidget(headerLabel);

    // 1. Group: Zoom & Navigation Controls
    auto* grpZoom = new QGroupBox("🔍 Zoom & Touchpad Sensitivity", this);
    auto* zoomLayout = new QVBoxLayout(grpZoom);
    zoomLayout->setSpacing(10);

    auto* sensRow = new QHBoxLayout();
    auto* lblSens = new QLabel("Two-Finger / Wheel Sensitivity:", grpZoom);
    lblSensitivityValue = new QLabel("50% (Smooth)", grpZoom);
    lblSensitivityValue->setStyleSheet("color: #8AB4F8; font-weight: bold;");
    sensRow->addWidget(lblSens);
    sensRow->addStretch();
    sensRow->addWidget(lblSensitivityValue);
    zoomLayout->addLayout(sensRow);

    sliderSensitivity = new QSlider(Qt::Horizontal, grpZoom);
    sliderSensitivity->setRange(10, 200); // 10% to 200%
    sliderSensitivity->setValue(50);
    sliderSensitivity->setTickPosition(QSlider::TicksBelow);
    sliderSensitivity->setTickInterval(25);
    zoomLayout->addWidget(sliderSensitivity);

    auto* sensHint = new QLabel("Tip: 30% - 50% is recommended for smooth, gentle laptop touchpads.", grpZoom);
    sensHint->setStyleSheet("font-size: 11px; color: #9AA0A6;");
    zoomLayout->addWidget(sensHint);

    chkAnchorCursor = new QCheckBox("Anchor Zoom to Mouse Cursor Position", grpZoom);
    chkAnchorCursor->setChecked(true);
    zoomLayout->addWidget(chkAnchorCursor);

    chkInvertScroll = new QCheckBox("Invert Zoom Scroll Direction", grpZoom);
    chkInvertScroll->setChecked(false);
    zoomLayout->addWidget(chkInvertScroll);

    mainLayout->addWidget(grpZoom);

    // 2. Group: Default Map & Theme
    auto* grpPreferences = new QGroupBox("🌐 Startup Preferences", this);
    auto* prefLayout = new QVBoxLayout(grpPreferences);
    prefLayout->setSpacing(8);

    auto* lblMode = new QLabel("Default Startup Map:", grpPreferences);
    lblMode->setStyleSheet("font-weight: bold; color: #FAFAFA;");
    prefLayout->addWidget(lblMode);

    auto* modeRow = new QHBoxLayout();
    radModeOnline = new QRadioButton("🌐 Online Map (Full India)", grpPreferences);
    radModeOffline = new QRadioButton("💾 Offline Map (Assam Local)", grpPreferences);
    modeRow->addWidget(radModeOnline);
    modeRow->addWidget(radModeOffline);
    prefLayout->addLayout(modeRow);

    auto* lblTheme = new QLabel("Default Theme:", grpPreferences);
    lblTheme->setStyleSheet("font-weight: bold; color: #FAFAFA;");
    prefLayout->addWidget(lblTheme);

    auto* themeRow = new QHBoxLayout();
    radThemeSystem = new QRadioButton("🖥️ System Default", grpPreferences);
    radThemeDark = new QRadioButton("🌙 Dark Theme", grpPreferences);
    radThemeLight = new QRadioButton("☀️ Light Theme", grpPreferences);
    themeRow->addWidget(radThemeSystem);
    themeRow->addWidget(radThemeDark);
    themeRow->addWidget(radThemeLight);
    prefLayout->addLayout(themeRow);

    mainLayout->addWidget(grpPreferences);

    // 3. Group: Performance & Memory
    auto* grpPerf = new QGroupBox("⚡ Performance & Display", this);
    auto* perfLayout = new QHBoxLayout(grpPerf);

    auto* lblCache = new QLabel("Tile Memory Cache:", grpPerf);
    cmbCacheSize = new QComboBox(grpPerf);
    cmbCacheSize->addItem("200 Tiles (~50 MB)", 200);
    cmbCacheSize->addItem("500 Tiles (~120 MB)", 500);
    cmbCacheSize->addItem("1000 Tiles (~250 MB)", 1000);
    cmbCacheSize->setCurrentIndex(1);

    chkShowMinimap = new QCheckBox("Show Minimap by Default", grpPerf);

    perfLayout->addWidget(lblCache);
    perfLayout->addWidget(cmbCacheSize);
    perfLayout->addSpacing(12);
    perfLayout->addWidget(chkShowMinimap);

    mainLayout->addWidget(grpPerf);

    // Action Buttons at bottom
    auto* buttonRow = new QHBoxLayout();
    buttonRow->setSpacing(10);

    btnReset = new QPushButton("🔄 Reset Defaults", this);
    btnClose = new QPushButton("Cancel", this);
    btnSave = new QPushButton("💾 Save & Apply", this);
    btnSave->setObjectName("btnSave");

    buttonRow->addWidget(btnReset);
    buttonRow->addStretch();
    buttonRow->addWidget(btnClose);
    buttonRow->addWidget(btnSave);

    mainLayout->addLayout(buttonRow);

    // Connect events
    connect(sliderSensitivity, &QSlider::valueChanged, this, &SettingsDialog::onSensitivityChanged);
    connect(btnSave, &QPushButton::clicked, this, &SettingsDialog::onSaveClicked);
    connect(btnReset, &QPushButton::clicked, this, &SettingsDialog::onResetDefaults);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::onSensitivityChanged(int value) {
    QString label;
    if (value <= 30) {
        label = QString("%1% (Ultra Smooth)").arg(value);
    } else if (value <= 60) {
        label = QString("%1% (Smooth / Touchpad)").arg(value);
    } else if (value <= 110) {
        label = QString("%1% (Normal / Mouse)").arg(value);
    } else {
        label = QString("%1% (Fast)").arg(value);
    }
    lblSensitivityValue->setText(label);
}

void SettingsDialog::loadSettings() {
    QSettings settings("SIH", "AssamMapExplorer");

    currentSettings.zoomSensitivity = settings.value("zoomSensitivity", 0.5).toDouble();
    currentSettings.anchorZoomToCursor = settings.value("anchorZoomToCursor", true).toBool();
    currentSettings.invertScroll = settings.value("invertScroll", false).toBool();
    currentSettings.startupMode = settings.value("startupMode", 0).toInt();
    currentSettings.startupTheme = settings.value("startupTheme", 0).toInt();
    currentSettings.tileCacheSize = settings.value("tileCacheSize", 500).toInt();
    currentSettings.showMinimap = settings.value("showMinimap", false).toBool();

    updateUIFromSettings();
}

void SettingsDialog::updateUIFromSettings() {
    int sliderVal = static_cast<int>(currentSettings.zoomSensitivity * 100.0);
    sliderSensitivity->setValue(std::clamp(sliderVal, 10, 200));
    onSensitivityChanged(sliderSensitivity->value());

    chkAnchorCursor->setChecked(currentSettings.anchorZoomToCursor);
    chkInvertScroll->setChecked(currentSettings.invertScroll);

    if (currentSettings.startupMode == 0) {
        radModeOnline->setChecked(true);
    } else {
        radModeOffline->setChecked(true);
    }

    if (currentSettings.startupTheme == 0) {
        radThemeSystem->setChecked(true);
    } else if (currentSettings.startupTheme == 1) {
        radThemeDark->setChecked(true);
    } else {
        radThemeLight->setChecked(true);
    }

    int cacheIdx = cmbCacheSize->findData(currentSettings.tileCacheSize);
    if (cacheIdx >= 0) cmbCacheSize->setCurrentIndex(cacheIdx);

    chkShowMinimap->setChecked(currentSettings.showMinimap);
}

void SettingsDialog::saveSettings() {
    currentSettings.zoomSensitivity = sliderSensitivity->value() / 100.0;
    currentSettings.anchorZoomToCursor = chkAnchorCursor->isChecked();
    currentSettings.invertScroll = chkInvertScroll->isChecked();
    currentSettings.startupMode = radModeOnline->isChecked() ? 0 : 1;
    currentSettings.startupTheme = radThemeSystem->isChecked() ? 0 : (radThemeDark->isChecked() ? 1 : 2);
    currentSettings.tileCacheSize = cmbCacheSize->currentData().toInt();
    currentSettings.showMinimap = chkShowMinimap->isChecked();

    QSettings settings("SIH", "AssamMapExplorer");
    settings.setValue("zoomSensitivity", currentSettings.zoomSensitivity);
    settings.setValue("anchorZoomToCursor", currentSettings.anchorZoomToCursor);
    settings.setValue("invertScroll", currentSettings.invertScroll);
    settings.setValue("startupMode", currentSettings.startupMode);
    settings.setValue("startupTheme", currentSettings.startupTheme);
    settings.setValue("tileCacheSize", currentSettings.tileCacheSize);
    settings.setValue("showMinimap", currentSettings.showMinimap);
}

void SettingsDialog::onSaveClicked() {
    saveSettings();
    emit settingsApplied(currentSettings);
    accept();
}

void SettingsDialog::onResetDefaults() {
    currentSettings = AppSettings(); // Reset to defaults (0.5 sensitivity)
    updateUIFromSettings();
}

} // namespace MapUI
