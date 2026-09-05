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

    // National Risk Summary Count Cards
    QLabel* m_lblCountSurveillance;
    QLabel* m_lblCountImminent;
    QLabel* m_lblCountWarning;
    QLabel* m_lblCountWatch;
    QLabel* m_lblCountNormal;
    QLabel* m_lblWeatherStatus;

    // Auto Surveillance Toolbar
    QPushButton* m_btnAutoMonitor;

    // Control Panel Widgets
    QLineEdit* m_txtSearchFilter;
    QComboBox* m_cboDamSelector;
    QDoubleSpinBox* m_spnRain1h;
    QDoubleSpinBox* m_spnRain24h;
    QDoubleSpinBox* m_spnFreeboard;
    QDoubleSpinBox* m_spnStorage;
    QDoubleSpinBox* m_spnCrestDisp;
    QComboBox* m_cboStructType;
    QPushButton* m_btnPredict;

    // Alert & Risk Telemetry HUD
    QLabel* m_lblDamTitle;
    QLabel* m_lblAlertBadge;
    QLabel* m_lblRiskProbPercent;
    QProgressBar* m_progressRisk;
    QLabel* m_lblOriStatus;
    QLabel* m_lblDbiStatus;
    QLabel* m_lblInflowSurge;
    QLabel* m_lblShapSummary;
    QLabel* m_lblActionGuide;

    // Monitored Dams Table
    QTableWidget* m_tblDams;

    MapCore::FloodSimulationState m_currentSimState;

    void setupUi();
    void buildControlPanel(QWidget* parent);
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
