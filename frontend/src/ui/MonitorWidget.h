#pragma once

#include <QWidget>
#include <QStackedWidget>
#include <QSplitter>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QSlider>
#include <QProgressBar>
#include <QTableWidget>
#include <QHeaderView>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLineEdit>
#include <QButtonGroup>
#include <QScrollArea>
#include <vector>

#include "OnlineTileWidget.h"
#include "TimelineWidget.h"
#include "../core/DamManager.h"
#include "../core/DamFloodSimulation.h"

namespace MapUI {

struct DamRiskAssessment {
    MapCore::DamPoint dam;
    double failureProbability = 0.0;
    QString alertLevel = "NORMAL"; // IMMINENT, WARNING, WATCH, NORMAL
    QString alertColor = "#22C55E";
    double ori = 0.0;
    double dbi = 3.5;
    double qIn = 320.0;
    double rain1h = 15.0;
    double rain24h = 85.0;
    double freeboard = 2.5;
    double storageMcm = 180.0;
    double crestDisp = 1.2;
    QString structType = "Earthfill";
    QString summary;
    QString triggerReason = "Routine Surveillance";
    bool isWeatherTriggered = false;
    QString source = "Modal Serverless GPU";
};

class MonitorWidget : public QWidget {
    Q_OBJECT

public:
    explicit MonitorWidget(MapCore::DamManager* damManager = nullptr, QWidget* parent = nullptr);
    ~MonitorWidget() override = default;

    void setDamManager(MapCore::DamManager* damManager);
    void setViewport(double lat, double lon, int zoom);
    void setSelectedDam(const MapCore::DamPoint& dam);
    void runStartupDangerAssessment();
    void fetchActiveSurveillanceDams();

signals:
    void viewportChanged(double lat, double lon, int zoom);

private slots:
    void onRunMlPrediction();
    void onDamSelected(int index);
    void onTimelineFrameChanged(int frame);
    void onNetworkReplyFinished(QNetworkReply* reply);
    void toggleAutoMonitor();
    void onAutoMonitorTick();
    void onScanNow();
    void onDamTableCellClicked(int row, int column);
    void onSearchFilterChanged(const QString& query);

private:
    MapCore::DamManager* m_damManager = nullptr;
    MapCore::DamPoint m_currentDam;
    QNetworkAccessManager* m_netManager = nullptr;

    // Auto Surveillance Timer
    QTimer* m_autoMonitorTimer = nullptr;
    bool m_autoMonitorActive = false;
    std::vector<DamRiskAssessment> m_allDamsRisk;

    // UI Layout Splitters
    QSplitter* m_mainSplitter;
    QSplitter* m_vSplitter;
    QWidget* m_mapContainer;
    OnlineTileWidget* m_onlineMap;
    TimelineWidget* m_timelineWidget;

    // Sidebar of Sidebar Navigation
    QWidget* m_iconStrip = nullptr;
    QButtonGroup* m_tabGroup = nullptr;
    QStackedWidget* m_pageStack = nullptr;
    QLabel* m_lblPageTitle = nullptr;

    QPushButton* m_btnTabBrief = nullptr;
    QPushButton* m_btnTabRadar = nullptr;
    QPushButton* m_btnTabControls = nullptr;
    QPushButton* m_btnTabPhysics = nullptr;
    QPushButton* m_btnTabProfile = nullptr;

    // National Risk Summary Count Cards
    QLabel* m_lblCountSurveillance = nullptr;
    QLabel* m_lblCountImminent = nullptr;
    QLabel* m_lblCountWarning = nullptr;
    QLabel* m_lblCountWatch = nullptr;
    QLabel* m_lblCountNormal = nullptr;
    QLabel* m_lblWeatherStatus = nullptr;

    // Auto Surveillance Toolbar
    QPushButton* m_btnAutoMonitor = nullptr;

    // Control Panel Widgets
    QLineEdit* m_txtSearchFilter = nullptr;
    QComboBox* m_cboDamSelector = nullptr;
    QDoubleSpinBox* m_spnRain1h = nullptr;
    QDoubleSpinBox* m_spnRain24h = nullptr;
    QDoubleSpinBox* m_spnFreeboard = nullptr;
    QDoubleSpinBox* m_spnStorage = nullptr;
    QDoubleSpinBox* m_spnCrestDisp = nullptr;
    QComboBox* m_cboStructType = nullptr;
    QPushButton* m_btnPredict = nullptr;

    // Alert & Risk Telemetry HUD
    QLabel* m_lblDamTitle = nullptr;
    QLabel* m_lblAlertBadge = nullptr;
    QLabel* m_lblRiskProbPercent = nullptr;
    QProgressBar* m_progressRisk = nullptr;
    QLabel* m_lblOriStatus = nullptr;
    QLabel* m_lblDbiStatus = nullptr;
    QLabel* m_lblInflowSurge = nullptr;
    QLabel* m_lblShapSummary = nullptr;
    QLabel* m_lblActionGuide = nullptr;

    // Brief Tab specific KPI labels
    QLabel* m_lblBriefRain = nullptr;
    QLabel* m_lblBriefInflow = nullptr;
    QLabel* m_lblBriefFreeboard = nullptr;
    QLabel* m_lblBriefDisp = nullptr;
    QLabel* m_lblBriefOri = nullptr;
    QLabel* m_lblBriefDbi = nullptr;
    QLabel* m_lblBriefShap = nullptr;

    // Dam Profile Tab specific labels
    QLabel* m_lblProfileName = nullptr;
    QLabel* m_lblProfilePic = nullptr;
    QLabel* m_lblProfileState = nullptr;
    QLabel* m_lblProfileRiver = nullptr;
    QLabel* m_lblProfileCoords = nullptr;
    QLabel* m_lblProfileHeight = nullptr;
    QLabel* m_lblProfileStorage = nullptr;
    QLabel* m_lblProfileType = nullptr;
    QLabel* m_lblProfileYear = nullptr;
    QLabel* m_lblProfilePurpose = nullptr;

    // Monitored Dams Table
    QTableWidget* m_tblDams = nullptr;

    MapCore::FloodSimulationState m_currentSimState;

    void setupUi();
    void buildControlPanel(QWidget* parent);
    QWidget* createBriefPage();
    QWidget* createRadarPage();
    QWidget* createControlsPage();
    QWidget* createPhysicsPage();
    QWidget* createProfilePage();

    void computeLocalPrediction();
    void fetchAllDamsLiveWeather();
    DamRiskAssessment evaluateDamRisk(const MapCore::DamPoint& dam, double r1h, double r24h, double fb, double storage, double crestDisp, const QString& structType);
    void updateMapDangerOverlays();
    void updateDamsTable();
    void selectDamAssessment(const DamRiskAssessment& risk);
    void updateHudUI(double probability, const QString& level, const QString& color,
                     const QString& summary, double ori, double dbi, double qIn);
};

} // namespace MapUI
