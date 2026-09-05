#pragma once

#include <QDialog>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QRadioButton>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSettings>

namespace MapUI {

struct AppSettings {
    double zoomSensitivity = 0.5; // 0.1 to 2.0 (0.5 is ideal for smooth two-finger touchpad)
    bool anchorZoomToCursor = true;
    bool invertScroll = false;
    int startupMode = 0; // 0 = Online, 1 = Offline
    int startupTheme = 0; // 0 = System, 1 = Dark, 2 = Light
    int tileCacheSize = 500;
    bool showMinimap = false;
};

class SettingsDialog : public QDialog {
    Q_OBJECT

private:
    AppSettings currentSettings;

    // UI Controls
    QSlider* sliderSensitivity;
    QLabel* lblSensitivityValue;
    QCheckBox* chkAnchorCursor;
    QCheckBox* chkInvertScroll;

    QRadioButton* radModeOnline;
    QRadioButton* radModeOffline;

    QRadioButton* radThemeSystem;
    QRadioButton* radThemeDark;
    QRadioButton* radThemeLight;

    QComboBox* cmbCacheSize;
    QCheckBox* chkShowMinimap;

    QPushButton* btnSave;
    QPushButton* btnReset;
    QPushButton* btnClose;

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    AppSettings getSettings() const { return currentSettings; }
    void loadSettings();
    void saveSettings();

signals:
    void settingsApplied(const AppSettings& settings);

private slots:
    void onSensitivityChanged(int value);
    void onSaveClicked();
    void onResetDefaults();

private:
    void setupUi();
    void updateUIFromSettings();
};

} // namespace MapUI
