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
#include <QProgressBar>
#include <vector>
#include <QSplitter>
#include "SidebarTerminal.h"
#include "../core/DamFloodSimulation.h"

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
    void setCollapsed(bool collapsed);

public slots:
    void toggleCollapse();
};

class PropertiesPanel : public QWidget {
    Q_OBJECT

    QWidget* iconStrip;
    QButtonGroup* tabButtonGroup;
    QStackedWidget* pageStack;
    QLabel* lblPanelTitle;

    QPushButton* btnTabFluid;
    QPushButton* btnTabTerrain;
    QPushButton* btnTabTelemetry;
    QPushButton* btnTabDisplay;
    QPushButton* btnTabDam;
    QPushButton* btnTabDanger;
    QPushButton* btnTabTerminal;

    SidebarTerminal* terminalWidget;
    QWidget* bottomWidget;
    QSplitter* sidebarSplitter;

    // Danger Tab members
    QLabel* lblDangerThreatLevel;
    QLabel* lblDangerSubStatus;
    QLabel* lblDangerActionNotice;
    QLabel* lblDangerStatusText;
    QLabel* lblDangerPopulationCount;
    QLabel* lblDangerInundatedCount;
    QLabel* lblDangerFrontEta;
    QLabel* lblDangerReachDist;
    QWidget* dangerCardsContainer;
    QVBoxLayout* dangerCardsLayout;
    std::vector<MapCore::DangerZone> currentDangerZones;

    // Fluid parameters
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

    // Real-Time Hydrodynamic Propagation & Pond/Spill Elevation Fields
    QLabel* lblHydroTime;
    QLabel* lblHydroBasin;
    QLabel* lblHydroElev;
    QLabel* lblHydroWSE;
    QLabel* lblHydroSpillStatus;
    QLabel* lblHydroPondVol;
    QLabel* lblHydroArea;
    QLabel* lblHydroFront;
    QLabel* lblHydroDepth;
    QLabel* lblHydroVel;
    QLabel* lblHydroDischarge;

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
    void updateHydrodynamicPropagation(int minute, double areaKm2, double frontDistKm, double maxDepthM, double maxVelMs, double peakDischargeQ,
                                       const QString& basinName = "", double bedZ = 0.0, double wse = 0.0, double saddleLipZ = 0.0,
                                       bool isOvertopping = false, double filledPct = 0.0, double totalPondedMCM = 0.0);
    void updateDangerZones(const std::vector<MapCore::DangerZone>& zones, int currentMinute, double frontDistKm);

    SidebarTerminal* getTerminal() { return terminalWidget; }
    void toggleTerminal();
    void setTerminalVisible(bool visible);
    bool isTerminalVisible() const;

signals:
    void simulationBakeRequested(double waterRise, double rainfall, double breachWidth);
    void playbackControlRequested(const QString& action);
    void jumpMinuteRequested(int minute);
    void parameterChanged(const QString& name, double val);
    void centerLocationRequested(double lat, double lon, int zoom);

private:
    void setupUi();
    QWidget* createFluidTab();
    QWidget* createTerrainTab();
    QWidget* createTelemetryTab();
    QWidget* createDisplayTab();
    QWidget* createDamTab();
    QWidget* createDangerTab();
    void switchTab(int index, const QString& title);
};

} // namespace MapUI
