#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QButtonGroup>
#include <vector>

namespace MapCore {
    struct DamPoint;
}

namespace MapUI {

class CollapsibleSection : public QWidget {
    Q_OBJECT

private:
    QPushButton* btnHeader;
    QWidget* contentWidget;
    QVBoxLayout* contentLayout;
    bool isCollapsed = false;

public:
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    QVBoxLayout* getContentLayout() { return contentLayout; }
    void addWidget(QWidget* w);

public slots:
    void toggleCollapse();
};

class PropertiesPanel : public QWidget {
    Q_OBJECT

private:
    QWidget* iconStrip;
    QButtonGroup* tabButtonGroup;
    QStackedWidget* pageStack;
    QLabel* lblPanelTitle;

    QPushButton* btnTabFluid;
    QPushButton* btnTabTerrain;
    QPushButton* btnTabTelemetry;
    QPushButton* btnTabDisplay;
    QPushButton* btnTabDam;

    // Simulation parameter inputs
    QDoubleSpinBox* spinWaterRise;
    QDoubleSpinBox* spinRainfall;
    QDoubleSpinBox* spinBreachWidth;
    QDoubleSpinBox* spinVelocity;

    QPushButton* btnBakeSim;
    QPushButton* btnResetSim;

    // Dam inspector fields
    QLabel* lblDamName;
    QLabel* lblDamPic;
    QLabel* lblDamStatus;
    QLabel* lblDamState;
    QLabel* lblDamDistrict;
    QLabel* lblDamRiver;
    QLabel* lblDamBasin;
    QLabel* lblDamCoords;
    QLabel* lblDamIncharge;

    QLabel* lblDamType;
    QLabel* lblDamHeight;
    QLabel* lblDamStorage;
    QLabel* lblDamSpillway;
    QLabel* lblDamYear;
    QLabel* lblDamPurpose;

    QDoubleSpinBox* spinDamWarningThreshold;
    QDoubleSpinBox* spinDamInflowMultiplier;
    QPushButton* btnCenterOnDam;
    QPushButton* btnSimulateDam;

    double currentDamLat = 0.0;
    double currentDamLon = 0.0;

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

    void showDamDetails(const MapCore::DamPoint& dam);
    void showDamSelectionSummary(int count, double minLat, double minLon, double maxLat, double maxLon, const std::vector<const MapCore::DamPoint*>& dams);

signals:
    void simulationBakeRequested(double waterRise, double rainfall, double breachWidth);
    void parameterChanged(const QString& name, double val);
    void centerLocationRequested(double lat, double lon, int zoom);

private:
    void setupUi();
    QWidget* createFluidTab();
    QWidget* createTerrainTab();
    QWidget* createTelemetryTab();
    QWidget* createDisplayTab();
    QWidget* createDamTab();
    void switchTab(int index, const QString& title);
};

} // namespace MapUI
