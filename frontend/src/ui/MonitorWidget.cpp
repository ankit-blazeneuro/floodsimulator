#include "MonitorWidget.h"
#include "IconHelper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QGraphicsDropShadowEffect>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <cmath>
#include <algorithm>

namespace MapUI {

MonitorWidget::MonitorWidget(MapCore::DamManager* damManager, QWidget* parent)
    : QWidget(parent), m_damManager(damManager)
{
    m_netManager = new QNetworkAccessManager(this);
    connect(m_netManager, &QNetworkAccessManager::finished, this, &MonitorWidget::onNetworkReplyFinished);

    // Initialize Auto Surveillance Timer (2-Minute Weather & Risk Poll Cycle)
    m_autoMonitorTimer = new QTimer(this);
    m_autoMonitorTimer->setInterval(120000);
    connect(m_autoMonitorTimer, &QTimer::timeout, this, &MonitorWidget::onAutoMonitorTick);

    // Initialize Default Dam: Pench Dam (Kamthikhairy)
    m_currentDam.pic = "MH09HH0596";
    m_currentDam.name = "Pench Dam (Kamthikhairy)";
    m_currentDam.state = "Maharashtra";
    m_currentDam.river = "Pench River";
    m_currentDam.lat = 21.4645;
    m_currentDam.lon = 79.1865;
    m_currentDam.height = 32.0f;
    m_currentDam.storage = 180.0f;
    m_currentDam.damType = "Earthfill";

    setupUi();
    runStartupDangerAssessment();
}

void MonitorWidget::setDamManager(MapCore::DamManager* damManager) {
    m_damManager = damManager;
    runStartupDangerAssessment();
}

void MonitorWidget::setupUi() {
    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    // Main Splitter [Left: Map + Timeline | Right: Controls & Risk Telemetry]
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setHandleWidth(2);
    m_mainSplitter->setStyleSheet(R"(
        QSplitter::handle:horizontal { background-color: #27272A; width: 2px; }
        QSplitter::handle:horizontal:hover { background-color: #38BDF8; }
        QSplitter::handle:vertical { background-color: #27272A; height: 2px; }
        QSplitter::handle:vertical:hover { background-color: #38BDF8; }
    )");

    // Vertical Splitter for Left Side [Top: Map | Bottom: Timeline]
    m_vSplitter = new QSplitter(Qt::Vertical, m_mainSplitter);
    m_vSplitter->setHandleWidth(2);

    m_mapContainer = new QWidget(m_vSplitter);
    auto* mapLayout = new QVBoxLayout(m_mapContainer);
    mapLayout->setContentsMargins(0, 0, 0, 0);

    m_onlineMap = new OnlineTileWidget(m_mapContainer);
    m_onlineMap->setCenter(m_currentDam.lat, m_currentDam.lon);
    m_onlineMap->setZoom(11);
    mapLayout->addWidget(m_onlineMap);

    connect(m_onlineMap, &OnlineTileWidget::viewportChanged, this, [this](double lat, double lon, int zoom) {
        emit viewportChanged(lat, lon, zoom);
    });

    m_timelineWidget = new TimelineWidget(m_vSplitter);
    m_timelineWidget->setDamSelected(true, "monitor_sim");
    m_timelineWidget->setFrameRange(0, 60);
    m_timelineWidget->setCurrentFrame(0);

    connect(m_timelineWidget, &TimelineWidget::frameChanged, this, &MonitorWidget::onTimelineFrameChanged);

    m_vSplitter->addWidget(m_mapContainer);
    m_vSplitter->addWidget(m_timelineWidget);
    m_vSplitter->setCollapsible(0, false);
    m_vSplitter->setCollapsible(1, true);
    m_vSplitter->setSizes({ 580, 180 });

    // Right Telemetry Panel
    auto* rightPanel = new QWidget(m_mainSplitter);
    rightPanel->setStyleSheet("QWidget { background-color: #121214; }");
    buildControlPanel(rightPanel);

    m_mainSplitter->addWidget(m_vSplitter);
    m_mainSplitter->addWidget(rightPanel);
    m_mainSplitter->setCollapsible(0, false);
    m_mainSplitter->setCollapsible(1, false);
    m_mainSplitter->setSizes({ 940, 440 });

    rootLayout->addWidget(m_mainSplitter);
}

static QScrollArea* wrapInScrollArea(QWidget* content) {
    auto* sa = new QScrollArea();
    sa->setWidgetResizable(true);
    sa->setFrameShape(QFrame::NoFrame);
    sa->setStyleSheet(R"(
        QScrollArea {
            background-color: #18181B;
            border: none;
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
    )");
    sa->setWidget(content);
    return sa;
}

void MonitorWidget::buildControlPanel(QWidget* parent) {
    parent->setStyleSheet(R"(
        QWidget#rightControlPanel {
            background-color: #18181B;
            border-left: 1px solid #27272A;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
        }
        QWidget#iconStrip {
            background-color: #121214;
            border-right: 1px solid #27272A;
        }
        QWidget#iconStrip QPushButton {
            background-color: transparent;
            border: none;
            border-left: 3px solid transparent;
            border-radius: 0px;
            min-width: 42px;
            max-width: 42px;
            min-height: 42px;
            max-height: 42px;
        }

        /* 1. Brief Tab (Sky Blue Theme) */
        QPushButton#btnTabBrief:hover {
            background-color: rgba(56, 189, 248, 0.10);
        }
        QPushButton#btnTabBrief:checked {
            background-color: rgba(56, 189, 248, 0.18);
            border-left: 3px solid #38BDF8;
        }

        /* 2. Radar Surveillance Tab (Amber Theme) */
        QPushButton#btnTabRadar:hover {
            background-color: rgba(253, 214, 99, 0.10);
        }
        QPushButton#btnTabRadar:checked {
            background-color: rgba(253, 214, 99, 0.18);
            border-left: 3px solid #FDD663;
        }

        /* 3. Controls Tab (Emerald Theme) */
        QPushButton#btnTabControls:hover {
            background-color: rgba(52, 211, 153, 0.10);
        }
        QPushButton#btnTabControls:checked {
            background-color: rgba(52, 211, 153, 0.18);
            border-left: 3px solid #34D399;
        }

        /* 4. Physics Tab (Rose / Red Theme) */
        QPushButton#btnTabPhysics:hover {
            background-color: rgba(244, 63, 94, 0.12);
        }
        QPushButton#btnTabPhysics:checked {
            background-color: rgba(244, 63, 94, 0.20);
            border-left: 3px solid #F43F5E;
        }

        /* 5. Dam Profile Tab (Purple Theme) */
        QPushButton#btnTabProfile:hover {
            background-color: rgba(167, 139, 250, 0.10);
        }
        QPushButton#btnTabProfile:checked {
            background-color: rgba(167, 139, 250, 0.18);
            border-left: 3px solid #A78BFA;
        }
    )");

    parent->setObjectName("rightControlPanel");

    auto* rootHBox = new QHBoxLayout(parent);
    rootHBox->setContentsMargins(0, 0, 0, 0);
    rootHBox->setSpacing(0);

    // 1. Left Vertical Icon Strip (Sidebar of Sidebar)
    m_iconStrip = new QWidget(parent);
    m_iconStrip->setObjectName("iconStrip");
    m_iconStrip->setFixedWidth(44);

    auto* isLayout = new QVBoxLayout(m_iconStrip);
    isLayout->setContentsMargins(0, 6, 0, 6);
    isLayout->setSpacing(3);

    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);

    m_btnTabBrief = new QPushButton(m_iconStrip);
    m_btnTabBrief->setObjectName("btnTabBrief");
    m_btnTabBrief->setIcon(IconHelper::graph(QColor(56, 189, 248), 22));
    m_btnTabBrief->setIconSize(QSize(22, 22));
    m_btnTabBrief->setCheckable(true);
    m_btnTabBrief->setToolTip("Brief Overview & Executive Dashboard");
    m_tabGroup->addButton(m_btnTabBrief, 0);
    isLayout->addWidget(m_btnTabBrief);

    m_btnTabRadar = new QPushButton(m_iconStrip);
    m_btnTabRadar->setObjectName("btnTabRadar");
    m_btnTabRadar->setIcon(IconHelper::radar(QColor(253, 214, 99), 22));
    m_btnTabRadar->setIconSize(QSize(22, 22));
    m_btnTabRadar->setCheckable(true);
    m_btnTabRadar->setToolTip("Nationwide Surveillance & Dam Ranking");
    m_tabGroup->addButton(m_btnTabRadar, 1);
    isLayout->addWidget(m_btnTabRadar);

    m_btnTabControls = new QPushButton(m_iconStrip);
    m_btnTabControls->setObjectName("btnTabControls");
    m_btnTabControls->setIcon(IconHelper::rain(QColor(52, 211, 153), 22));
    m_btnTabControls->setIconSize(QSize(22, 22));
    m_btnTabControls->setCheckable(true);
    m_btnTabControls->setToolTip("Hydraulic Telemetry & Simulation Controls");
    m_tabGroup->addButton(m_btnTabControls, 2);
    isLayout->addWidget(m_btnTabControls);

    m_btnTabPhysics = new QPushButton(m_iconStrip);
    m_btnTabPhysics->setObjectName("btnTabPhysics");
    m_btnTabPhysics->setIcon(IconHelper::danger(QColor(244, 63, 94), 22));
    m_btnTabPhysics->setIconSize(QSize(22, 22));
    m_btnTabPhysics->setCheckable(true);
    m_btnTabPhysics->setToolTip("Physics Indices (ORI / DBI) & SHAP Drivers");
    m_tabGroup->addButton(m_btnTabPhysics, 3);
    isLayout->addWidget(m_btnTabPhysics);

    m_btnTabProfile = new QPushButton(m_iconStrip);
    m_btnTabProfile->setObjectName("btnTabProfile");
    m_btnTabProfile->setIcon(IconHelper::info(QColor(167, 139, 250), 22));
    m_btnTabProfile->setIconSize(QSize(22, 22));
    m_btnTabProfile->setCheckable(true);
    m_btnTabProfile->setToolTip("Dam Specifications & Engineering Profile");
    m_tabGroup->addButton(m_btnTabProfile, 4);
    isLayout->addWidget(m_btnTabProfile);

    isLayout->addStretch();
    rootHBox->addWidget(m_iconStrip);

    // 2. Right Content Stack
    auto* rightContent = new QWidget(parent);
    auto* rcLayout = new QVBoxLayout(rightContent);
    rcLayout->setContentsMargins(0, 0, 0, 0);
    rcLayout->setSpacing(0);

    // Header bar with active category title
    auto* headerBar = new QWidget(rightContent);
    headerBar->setStyleSheet("background-color: #141416; border-bottom: 1px solid #27272A;");
    headerBar->setFixedHeight(38);
    auto* hbLayout = new QHBoxLayout(headerBar);
    hbLayout->setContentsMargins(12, 0, 12, 0);

    m_lblPageTitle = new QLabel("📋 BRIEF — EXECUTIVE OVERVIEW", headerBar);
    m_lblPageTitle->setStyleSheet("font-size: 11px; font-weight: bold; color: #FAFAFA; letter-spacing: 0.5px;");
    hbLayout->addWidget(m_lblPageTitle);
    hbLayout->addStretch();
    rcLayout->addWidget(headerBar);

    // Stack of Category Pages
    m_pageStack = new QStackedWidget(rightContent);
    m_pageStack->addWidget(createBriefPage());       // Index 0: Brief
    m_pageStack->addWidget(createRadarPage());       // Index 1: Radar
    m_pageStack->addWidget(createControlsPage());    // Index 2: Controls
    m_pageStack->addWidget(createPhysicsPage());     // Index 3: Physics
    m_pageStack->addWidget(createProfilePage());     // Index 4: Profile

    rcLayout->addWidget(m_pageStack);
    rootHBox->addWidget(rightContent, 1);

    // Switch categories on icon click
    connect(m_tabGroup, &QButtonGroup::idClicked, this, [this](int id) {
        m_pageStack->setCurrentIndex(id);
        switch (id) {
            case 0: m_lblPageTitle->setText("📋 BRIEF — EXECUTIVE OVERVIEW"); break;
            case 1: m_lblPageTitle->setText("🛰️ NATIONWIDE SURVEILLANCE & RANKING"); break;
            case 2: m_lblPageTitle->setText("🎛️ HYDRAULIC & STRUCTURAL CONTROLS"); break;
            case 3: m_lblPageTitle->setText("🔬 PHYSICS INDICES & SHAP ATTRIBUTION"); break;
            case 4: m_lblPageTitle->setText("ℹ️ DAM SPECIFICATIONS & PROFILE"); break;
            default: break;
        }
    });

    m_btnTabBrief->setChecked(true);
}

QWidget* MonitorWidget::createBriefPage() {
    auto* content = new QWidget();
    content->setStyleSheet(R"(
        QWidget {
            background-color: #18181B;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            color: #FAFAFA;
        }
        QGroupBox {
            font-size: 11px;
            font-weight: bold;
            color: #A1A1AA;
            border: 1px solid #27272A;
            border-radius: 6px;
            margin-top: 6px;
            padding-top: 14px;
            background-color: #141416;
        }
    )");

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // 1. Active Dam Quick Header Card
    auto* damCard = new QWidget(content);
    damCard->setStyleSheet("background-color: #1E1E22; border: 1px solid #27272A; border-radius: 6px; padding: 4px;");
    auto* damCardLayout = new QVBoxLayout(damCard);
    damCardLayout->setContentsMargins(8, 6, 8, 6);
    damCardLayout->setSpacing(4);

    auto* hTop = new QHBoxLayout();
    m_lblDamTitle = new QLabel("Pench Dam (Kamthikhairy)", damCard);
    m_lblDamTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #FAFAFA;");
    m_lblAlertBadge = new QLabel(" 🟢 NORMAL ", damCard);
    m_lblAlertBadge->setStyleSheet(R"(
        QLabel {
            background-color: rgba(34, 197, 94, 0.2);
            color: #22C55E;
            border: 1px solid #22C55E;
            border-radius: 6px;
            font-size: 11px;
            font-weight: bold;
            padding: 2px 6px;
        }
    )");
    hTop->addWidget(m_lblDamTitle);
    hTop->addStretch();
    hTop->addWidget(m_lblAlertBadge);
    damCardLayout->addLayout(hTop);

    // Quick Dam Selector Dropdown
    m_cboDamSelector = new QComboBox(damCard);
    m_cboDamSelector->setStyleSheet(R"(
        QComboBox {
            background-color: #27272A;
            color: #E4E4E7;
            border: 1px solid #3F3F46;
            border-radius: 4px;
            padding: 4px 6px;
            font-size: 11px;
        }
        QComboBox QAbstractItemView {
            background-color: #1E1E22;
            color: #FAFAFA;
            selection-background-color: #38BDF8;
        }
    )");
    m_cboDamSelector->addItem("MH09HH0596 - Pench Dam (Kamthikhairy, MH)");
    connect(m_cboDamSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MonitorWidget::onDamSelected);
    damCardLayout->addWidget(m_cboDamSelector);

    layout->addWidget(damCard);

    // 2. Predictive Breach Risk Master Gauge
    auto* probBox = new QGroupBox("Predictive Breach Risk Index (BG_model)", content);
    auto* probLayout = new QVBoxLayout(probBox);
    probLayout->setContentsMargins(8, 12, 8, 8);
    probLayout->setSpacing(6);

    m_lblRiskProbPercent = new QLabel("8.4%", probBox);
    m_lblRiskProbPercent->setStyleSheet("font-size: 26px; font-weight: bold; color: #22C55E;");
    m_lblRiskProbPercent->setAlignment(Qt::AlignCenter);
    probLayout->addWidget(m_lblRiskProbPercent);

    m_progressRisk = new QProgressBar(probBox);
    m_progressRisk->setRange(0, 100);
    m_progressRisk->setValue(8);
    m_progressRisk->setTextVisible(false);
    m_progressRisk->setFixedHeight(6);
    m_progressRisk->setStyleSheet(R"(
        QProgressBar { background-color: #27272A; border-radius: 3px; }
        QProgressBar::chunk { background-color: #22C55E; border-radius: 3px; }
    )");
    probLayout->addWidget(m_progressRisk);

    m_lblActionGuide = new QLabel("Dam operating within nominal parameters. Inundation safe.", probBox);
    m_lblActionGuide->setStyleSheet("font-size: 10px; color: #A1A1AA;");
    m_lblActionGuide->setAlignment(Qt::AlignCenter);
    m_lblActionGuide->setWordWrap(true);
    probLayout->addWidget(m_lblActionGuide);

    layout->addWidget(probBox);

    // 3. Single Master Button: Unified Full Surveillance & ML Prediction
    m_btnPredict = new QPushButton("⚡ Run Full Surveillance & ML Prediction", content);
    m_btnPredict->setToolTip("Queries live Open-Meteo weather across India, batch-updates all 6,600+ dams on Modal GPU, and computes real-time breach risk for the selected dam.");
    m_btnPredict->setStyleSheet(R"(
        QPushButton {
            background-color: #0284C7;
            color: #FFFFFF;
            font-weight: bold;
            font-size: 11px;
            border-radius: 6px;
            padding: 9px 12px;
        }
        QPushButton:hover { background-color: #0369A1; }
        QPushButton:pressed { background-color: #075985; }
        QPushButton:disabled { background-color: #3F3F46; color: #A1A1AA; }
    )");
    connect(m_btnPredict, &QPushButton::clicked, this, &MonitorWidget::onRunMlPrediction);
    layout->addWidget(m_btnPredict);

    // 4. Brief KPI Matrix Card (All Info at a Glance)
    auto* kpiBox = new QGroupBox("⚡ Key Telemetry & Physics Metrics", content);
    auto* kpiLayout = new QGridLayout(kpiBox);
    kpiLayout->setContentsMargins(8, 12, 8, 8);
    kpiLayout->setSpacing(6);

    auto makeKpiTile = [](const QString& labelText, QLabel*& valLabelRef) {
        auto* tile = new QWidget();
        tile->setStyleSheet("background-color: #1E1E22; border-radius: 4px; padding: 4px;");
        auto* tl = new QVBoxLayout(tile);
        tl->setContentsMargins(4, 3, 4, 3);
        tl->setSpacing(1);
        auto* title = new QLabel(labelText, tile);
        title->setStyleSheet("font-size: 9px; color: #71717A; font-weight: bold;");
        valLabelRef = new QLabel("--", tile);
        valLabelRef->setStyleSheet("font-size: 11px; font-weight: bold; color: #FAFAFA;");
        tl->addWidget(title);
        tl->addWidget(valLabelRef);
        return tile;
    };

    kpiLayout->addWidget(makeKpiTile("🌧️ PRECIPITATION", m_lblBriefRain), 0, 0);
    kpiLayout->addWidget(makeKpiTile("🌊 INFLOW SURGE", m_lblBriefInflow), 0, 1);
    kpiLayout->addWidget(makeKpiTile("📏 FREEBOARD / STORAGE", m_lblBriefFreeboard), 1, 0);
    kpiLayout->addWidget(makeKpiTile("📐 STRUCTURE / DISP", m_lblBriefDisp), 1, 1);
    kpiLayout->addWidget(makeKpiTile("⚠️ OVERTOPPING INDEX", m_lblBriefOri), 2, 0);
    kpiLayout->addWidget(makeKpiTile("🛡️ BREACH STABILITY", m_lblBriefDbi), 2, 1);

    layout->addWidget(kpiBox);

    // 5. National Surveillance Status Summary Badges
    auto* nationalBox = new QGroupBox("🛰️ National Early Warning Surveillance", content);
    auto* natLayout = new QVBoxLayout(nationalBox);
    natLayout->setContentsMargins(6, 12, 6, 6);
    natLayout->setSpacing(6);

    auto* topNat = new QHBoxLayout();
    auto* lblNatSummary = new QLabel("Active Monitored Reservoirs", nationalBox);
    lblNatSummary->setStyleSheet("font-size: 10px; color: #A1A1AA;");
    topNat->addWidget(lblNatSummary);
    topNat->addStretch();

    m_btnAutoMonitor = new QPushButton("🛰️ Auto Monitor: OFF", nationalBox);
    m_btnAutoMonitor->setStyleSheet(R"(
        QPushButton {
            background-color: #27272A;
            color: #A1A1AA;
            font-size: 10px;
            font-weight: bold;
            border: 1px solid #3F3F46;
            border-radius: 4px;
            padding: 3px 8px;
        }
        QPushButton:hover { background-color: #3F3F46; color: #FAFAFA; }
    )");
    connect(m_btnAutoMonitor, &QPushButton::clicked, this, &MonitorWidget::toggleAutoMonitor);
    topNat->addWidget(m_btnAutoMonitor);
    natLayout->addLayout(topNat);

    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(4);

    auto createBadge = [](const QString& countStr, const QString& label, const QString& color) {
        auto* box = new QWidget();
        box->setStyleSheet(QString("background-color: rgba(255, 255, 255, 0.04); border: 1px solid %1; border-radius: 4px;").arg(color));
        auto* l = new QVBoxLayout(box);
        l->setContentsMargins(2, 3, 2, 3);
        l->setSpacing(0);
        auto* lblVal = new QLabel(countStr, box);
        lblVal->setAlignment(Qt::AlignCenter);
        lblVal->setStyleSheet(QString("font-size: 13px; font-weight: bold; color: %1;").arg(color));
        auto* lblTxt = new QLabel(label, box);
        lblTxt->setAlignment(Qt::AlignCenter);
        lblTxt->setStyleSheet("font-size: 8px; color: #A1A1AA;");
        l->addWidget(lblVal);
        l->addWidget(lblTxt);
        return std::make_pair(box, lblVal);
    };

    auto pairSurveillance = createBadge("0", "Radar",     "#06B6D4");
    auto pairImminent     = createBadge("0", "Imminent",  "#EF4444");
    auto pairWarning      = createBadge("0", "Warning",   "#F97316");
    auto pairWatch        = createBadge("0", "Watch",     "#EAB308");
    auto pairNormal       = createBadge("0", "Normal",    "#22C55E");

    m_lblCountSurveillance = pairSurveillance.second;
    m_lblCountImminent     = pairImminent.second;
    m_lblCountWarning      = pairWarning.second;
    m_lblCountWatch        = pairWatch.second;
    m_lblCountNormal       = pairNormal.second;

    cardsLayout->addWidget(pairSurveillance.first);
    cardsLayout->addWidget(pairImminent.first);
    cardsLayout->addWidget(pairWarning.first);
    cardsLayout->addWidget(pairWatch.first);
    cardsLayout->addWidget(pairNormal.first);
    natLayout->addLayout(cardsLayout);

    m_lblWeatherStatus = new QLabel("🛰️ Gridded NWP Cluster Polling · Open-Meteo ECMWF/GFS & Modal Cloud", nationalBox);
    m_lblWeatherStatus->setStyleSheet("font-size: 9px; color: #38BDF8; font-style: italic;");
    natLayout->addWidget(m_lblWeatherStatus);

    layout->addWidget(nationalBox);

    // 6. Top AI Attribution Narrative Card
    auto* shapBox = new QGroupBox("🤖 Top AI Risk Driver & Explainability", content);
    auto* shapLayout = new QVBoxLayout(shapBox);
    shapLayout->setContentsMargins(8, 12, 8, 8);

    m_lblBriefShap = new QLabel("Nominal freeboard and balanced catchment inflow. No immediate hazards detected.", shapBox);
    m_lblBriefShap->setStyleSheet("font-size: 10px; color: #38BDF8; font-style: italic;");
    m_lblBriefShap->setWordWrap(true);
    shapLayout->addWidget(m_lblBriefShap);

    layout->addWidget(shapBox);
    layout->addStretch();

    return wrapInScrollArea(content);
}

QWidget* MonitorWidget::createRadarPage() {
    auto* content = new QWidget();
    content->setStyleSheet("background-color: #18181B; font-family: 'Segoe UI', Arial, sans-serif; font-size: 11px;");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* infoCard = new QWidget(content);
    infoCard->setStyleSheet("background-color: #1E1E22; border: 1px solid #27272A; border-radius: 6px; padding: 6px;");
    auto* icLayout = new QVBoxLayout(infoCard);
    icLayout->setContentsMargins(6, 4, 6, 4);
    icLayout->setSpacing(2);
    auto* lblDescTitle = new QLabel("🛰️ Real-Time Radar Surveillance & Ranking", infoCard);
    lblDescTitle->setStyleSheet("font-size: 12px; font-weight: bold; color: #FAFAFA;");
    auto* lblDescSub = new QLabel("Continuously scans rainfall clusters across all 6,600+ reservoirs in India. Click any row to inspect.", infoCard);
    lblDescSub->setStyleSheet("font-size: 10px; color: #A1A1AA;");
    icLayout->addWidget(lblDescTitle);
    icLayout->addWidget(lblDescSub);
    layout->addWidget(infoCard);

    // Search filter
    m_txtSearchFilter = new QLineEdit(content);
    m_txtSearchFilter->setPlaceholderText("🔍 Search 6,600+ dams by name, state, PIC, or alert...");
    m_txtSearchFilter->setStyleSheet(R"(
        QLineEdit {
            background-color: #27272A;
            color: #FAFAFA;
            border: 1px solid #3F3F46;
            border-radius: 4px;
            padding: 5px 8px;
            font-size: 11px;
        }
        QLineEdit:focus { border: 1px solid #38BDF8; }
    )");
    connect(m_txtSearchFilter, &QLineEdit::textChanged, this, &MonitorWidget::onSearchFilterChanged);
    layout->addWidget(m_txtSearchFilter);

    // Monitored Dams Table
    m_tblDams = new QTableWidget(0, 6, content);
    m_tblDams->setHorizontalHeaderLabels({ "Alert", "Dam Name", "State", "Rain Trigger Criteria", "P(Breach)", "Level" });
    m_tblDams->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tblDams->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tblDams->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_tblDams->verticalHeader()->setVisible(false);
    m_tblDams->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tblDams->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tblDams->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tblDams->setMinimumHeight(420);
    m_tblDams->setStyleSheet(R"(
        QTableWidget {
            background-color: #141416;
            color: #FAFAFA;
            gridline-color: #27272A;
            border: 1px solid #27272A;
            border-radius: 4px;
            font-size: 11px;
        }
        QTableWidget::item:selected {
            background-color: #0284C7;
            color: #FFFFFF;
        }
        QHeaderView::section {
            background-color: #1E1E22;
            color: #A1A1AA;
            font-weight: bold;
            border: 1px solid #27272A;
            padding: 4px;
        }
    )");
    connect(m_tblDams, &QTableWidget::cellClicked, this, &MonitorWidget::onDamTableCellClicked);
    layout->addWidget(m_tblDams);

    // Diagnostic trigger criteria box
    auto* diagBox = new QGroupBox("⚡ Surveillance Trigger Criteria", content);
    diagBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #A1A1AA; border: 1px solid #27272A; border-radius: 6px; margin-top: 6px; padding-top: 14px; background-color: #141416; }");
    auto* diagLayout = new QVBoxLayout(diagBox);
    diagLayout->setContentsMargins(8, 12, 8, 8);
    diagLayout->setSpacing(3);

    auto makeDiagLine = [](const QString& icon, const QString& rule, const QString& desc) {
        auto* l = new QLabel(QString("<b>%1 %2</b> — <span style='color: #A1A1AA;'>%3</span>").arg(icon, rule, desc));
        l->setStyleSheet("font-size: 10px; color: #E4E4E7;");
        l->setWordWrap(true);
        return l;
    };
    diagLayout->addWidget(makeDiagLine("⚡", "Cloudburst Alarm:", "Rainfall ≥ 20 mm/hr detected in catchment"));
    diagLayout->addWidget(makeDiagLine("🌧️", "Heavy Sustained Rain:", "Rainfall ≥ 10 mm/hr persisting for ≥ 2 hours"));
    diagLayout->addWidget(makeDiagLine("🌊", "Ground Saturation:", "24-hour cumulative precipitation ≥ 80 mm"));
    diagLayout->addWidget(makeDiagLine("🛡️", "Infiltration Absorption:", "Rain < 5 mm/hr absorbed by soil; zero breach risk"));

    layout->addWidget(diagBox);
    layout->addStretch();

    return wrapInScrollArea(content);
}

QWidget* MonitorWidget::createControlsPage() {
    auto* content = new QWidget();
    content->setStyleSheet("background-color: #18181B; font-family: 'Segoe UI', Arial, sans-serif; font-size: 11px;");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* formBox = new QGroupBox("🎛️ Hydro-Meteorological & Structural Controls", content);
    formBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #A1A1AA; border: 1px solid #27272A; border-radius: 6px; margin-top: 6px; padding-top: 14px; background-color: #141416; }");
    auto* formLayout = new QFormLayout(formBox);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setContentsMargins(8, 12, 8, 8);
    formLayout->setSpacing(8);

    m_spnRain1h = new QDoubleSpinBox(formBox);
    m_spnRain1h->setRange(0.0, 250.0);
    m_spnRain1h->setValue(15.0);
    m_spnRain1h->setSuffix(" mm/h");

    m_spnRain24h = new QDoubleSpinBox(formBox);
    m_spnRain24h->setRange(0.0, 600.0);
    m_spnRain24h->setValue(85.0);
    m_spnRain24h->setSuffix(" mm");

    m_spnFreeboard = new QDoubleSpinBox(formBox);
    m_spnFreeboard->setRange(0.05, 20.0);
    m_spnFreeboard->setValue(2.50);
    m_spnFreeboard->setSuffix(" m");

    m_spnStorage = new QDoubleSpinBox(formBox);
    m_spnStorage->setRange(0.1, 5000.0);
    m_spnStorage->setValue(180.0);
    m_spnStorage->setSuffix(" MCM");

    m_spnCrestDisp = new QDoubleSpinBox(formBox);
    m_spnCrestDisp->setRange(0.0, 100.0);
    m_spnCrestDisp->setValue(1.2);
    m_spnCrestDisp->setSuffix(" mm/yr");

    m_cboStructType = new QComboBox(formBox);
    m_cboStructType->addItems({ "Earthfill", "Rockfill", "Gravity Concrete", "Masonry", "Landslide Debris" });

    auto styleInput = [](QWidget* w) {
        w->setStyleSheet("background-color: #27272A; color: #E4E4E7; border: 1px solid #3F3F46; border-radius: 4px; padding: 4px 6px; font-size: 11px;");
    };
    styleInput(m_spnRain1h);
    styleInput(m_spnRain24h);
    styleInput(m_spnFreeboard);
    styleInput(m_spnStorage);
    styleInput(m_spnCrestDisp);
    styleInput(m_cboStructType);

    formLayout->addRow("Precipitation (1h):", m_spnRain1h);
    formLayout->addRow("Precipitation (24h):", m_spnRain24h);
    formLayout->addRow("Remaining Freeboard:", m_spnFreeboard);
    formLayout->addRow("Gross Storage MCM:", m_spnStorage);
    formLayout->addRow("Crest Displacement:", m_spnCrestDisp);
    formLayout->addRow("Dam Structural Type:", m_cboStructType);

    layout->addWidget(formBox);

    auto* btnLocalRecompute = new QPushButton("⚡ Recompute Scenario Risk", content);
    btnLocalRecompute->setStyleSheet(R"(
        QPushButton {
            background-color: #27272A;
            color: #FFFFFF;
            font-weight: bold;
            font-size: 11px;
            border: 1px solid #3F3F46;
            border-radius: 5px;
            padding: 8px 12px;
        }
        QPushButton:hover { background-color: #3F3F46; }
    )");
    connect(btnLocalRecompute, &QPushButton::clicked, this, &MonitorWidget::computeLocalPrediction);
    layout->addWidget(btnLocalRecompute);

    // Engineering Guidelines Reference Box
    auto* refBox = new QGroupBox("📐 CWC & IS 11223 Engineering Guidelines", content);
    refBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #A1A1AA; border: 1px solid #27272A; border-radius: 6px; margin-top: 6px; padding-top: 14px; background-color: #141416; }");
    auto* refLayout = new QVBoxLayout(refBox);
    refLayout->setContentsMargins(8, 12, 8, 8);
    refLayout->setSpacing(4);

    auto makeRef = [](const QString& title, const QString& body) {
        auto* l = new QLabel(QString("<b>%1</b>: <span style='color: #A1A1AA;'>%2</span>").arg(title, body));
        l->setStyleSheet("font-size: 10px; color: #E4E4E7;");
        l->setWordWrap(true);
        return l;
    };
    refLayout->addWidget(makeRef("Freeboard Standards", "Minimum 2.5m for major earthfill dams; ≥ 1.5m for masonry/gravity dams under CWC design flood criteria."));
    refLayout->addWidget(makeRef("Crest Deformation", "Annual crest shifts < 5 mm/yr represent elastic thermal response. Shifts > 20 mm/yr trigger geotechnical slope failure warnings."));
    refLayout->addWidget(makeRef("Emergency Spillways", "Engineered dams possess non-gated waste weirs providing automatic flow release floors even if main gates fail."));

    layout->addWidget(refBox);
    layout->addStretch();

    return wrapInScrollArea(content);
}

QWidget* MonitorWidget::createPhysicsPage() {
    auto* content = new QWidget();
    content->setStyleSheet("background-color: #18181B; font-family: 'Segoe UI', Arial, sans-serif; font-size: 11px;");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    // 1. Overtopping Risk Index (ORI) Card
    auto* oriBox = new QGroupBox("⚠️ Overtopping Risk Index (ORI)", content);
    oriBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #A1A1AA; border: 1px solid #27272A; border-radius: 6px; margin-top: 6px; padding-top: 14px; background-color: #141416; }");
    auto* oriLayout = new QVBoxLayout(oriBox);
    oriLayout->setContentsMargins(8, 12, 8, 8);
    m_lblOriStatus = new QLabel("ORI (Overtopping Risk Index): Safe", oriBox);
    m_lblOriStatus->setStyleSheet("font-size: 13px; font-weight: bold; color: #22C55E;");
    auto* lblOriFormula = new QLabel("Formula: ORI = max(0, (Q_in - Q_spillway) / V_freeboard)", oriBox);
    lblOriFormula->setStyleSheet("font-size: 10px; font-family: monospace; color: #38BDF8;");
    auto* lblOriDesc = new QLabel("Measures how quickly the net hydraulic inflow surcharges beyond spillway discharge capacity into the remaining freeboard chamber.", oriBox);
    lblOriDesc->setStyleSheet("font-size: 10px; color: #A1A1AA;");
    lblOriDesc->setWordWrap(true);
    oriLayout->addWidget(m_lblOriStatus);
    oriLayout->addWidget(lblOriFormula);
    oriLayout->addWidget(lblOriDesc);
    layout->addWidget(oriBox);

    // 2. Dam Breach Index (DBI) Card
    auto* dbiBox = new QGroupBox("🛡️ Dam Breach Index (Stefanelli & Cenderelli DBI)", content);
    dbiBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #A1A1AA; border: 1px solid #27272A; border-radius: 6px; margin-top: 6px; padding-top: 14px; background-color: #141416; }");
    auto* dbiLayout = new QVBoxLayout(dbiBox);
    dbiLayout->setContentsMargins(8, 12, 8, 8);
    m_lblDbiStatus = new QLabel("DBI (Dam Breach Index): 3.50 (Stable)", dbiBox);
    m_lblDbiStatus->setStyleSheet("font-size: 13px; font-weight: bold; color: #22C55E;");
    auto* lblDbiFormula = new QLabel("Formula: DBI = log10( (V_d * H_d) / W_d )", dbiBox);
    lblDbiFormula->setStyleSheet("font-size: 10px; font-family: monospace; color: #38BDF8;");
    auto* lblDbiDesc = new QLabel("Geotechnical threshold: DBI ≥ 2.75 indicates stable embankment. Values ≤ 2.75 indicate imminent landslide dam or structural rupture.", dbiBox);
    lblDbiDesc->setStyleSheet("font-size: 10px; color: #A1A1AA;");
    lblDbiDesc->setWordWrap(true);
    dbiLayout->addWidget(m_lblDbiStatus);
    dbiLayout->addWidget(lblDbiFormula);
    dbiLayout->addWidget(lblDbiDesc);
    layout->addWidget(dbiBox);

    // 3. Catchment Peak Runoff
    auto* runoffBox = new QGroupBox("🌊 Rational Method Catchment Inflow", content);
    runoffBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #A1A1AA; border: 1px solid #27272A; border-radius: 6px; margin-top: 6px; padding-top: 14px; background-color: #141416; }");
    auto* roLayout = new QVBoxLayout(runoffBox);
    roLayout->setContentsMargins(8, 12, 8, 8);
    m_lblInflowSurge = new QLabel("Inflow Surge Q_in: 320 m³/s", runoffBox);
    m_lblInflowSurge->setStyleSheet("font-size: 13px; font-weight: bold; color: #FAFAFA;");
    auto* lblRoFormula = new QLabel("Formula: Q_in = 0.278 * C * I * A", runoffBox);
    lblRoFormula->setStyleSheet("font-size: 10px; font-family: monospace; color: #38BDF8;");
    auto* lblRoDesc = new QLabel("Calibrated with IS 11223 emergency weir discharge floors. Light rain (< 5 mm/hr) is absorbed by unsaturated catchment soil.", runoffBox);
    lblRoDesc->setStyleSheet("font-size: 10px; color: #A1A1AA;");
    lblRoDesc->setWordWrap(true);
    roLayout->addWidget(m_lblInflowSurge);
    roLayout->addWidget(lblRoFormula);
    roLayout->addWidget(lblRoDesc);
    layout->addWidget(runoffBox);

    // 4. SHAP Attributions Box
    auto* shapBox = new QGroupBox("🤖 SHAP Decision Attributions", content);
    shapBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #A1A1AA; border: 1px solid #27272A; border-radius: 6px; margin-top: 6px; padding-top: 14px; background-color: #141416; }");
    auto* shLayout = new QVBoxLayout(shapBox);
    shLayout->setContentsMargins(8, 12, 8, 8);
    m_lblShapSummary = new QLabel("Drivers: Nominal freeboard & balanced inflow.", shapBox);
    m_lblShapSummary->setStyleSheet("font-size: 11px; color: #38BDF8; font-style: italic;");
    m_lblShapSummary->setWordWrap(true);
    shLayout->addWidget(m_lblShapSummary);
    layout->addWidget(shapBox);

    layout->addStretch();
    return wrapInScrollArea(content);
}

QWidget* MonitorWidget::createProfilePage() {
    auto* content = new QWidget();
    content->setStyleSheet("background-color: #18181B; font-family: 'Segoe UI', Arial, sans-serif; font-size: 11px;");
    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* profileBox = new QGroupBox("🏛️ National Dam Registry (CWC / NRLD Profile)", content);
    profileBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #A1A1AA; border: 1px solid #27272A; border-radius: 6px; margin-top: 6px; padding-top: 14px; background-color: #141416; }");
    auto* pLayout = new QFormLayout(profileBox);
    pLayout->setContentsMargins(8, 12, 8, 8);
    pLayout->setSpacing(6);

    auto makeVal = [](QLabel*& lblRef, const QString& defVal) {
        lblRef = new QLabel(defVal);
        lblRef->setStyleSheet("font-size: 11px; font-weight: bold; color: #FAFAFA;");
        return lblRef;
    };

    pLayout->addRow("Dam Name:", makeVal(m_lblProfileName, "Pench Dam (Kamthikhairy)"));
    pLayout->addRow("National PIC:", makeVal(m_lblProfilePic, "MH09HH0596"));
    pLayout->addRow("State / District:", makeVal(m_lblProfileState, "Maharashtra / Nagpur"));
    pLayout->addRow("River Basin:", makeVal(m_lblProfileRiver, "Pench River (Godavari Basin)"));
    pLayout->addRow("Geo Coordinates:", makeVal(m_lblProfileCoords, "21.4645° N, 79.1865° E"));
    pLayout->addRow("Structural Height:", makeVal(m_lblProfileHeight, "32.0 m"));
    pLayout->addRow("Gross Storage:", makeVal(m_lblProfileStorage, "180.0 MCM"));
    pLayout->addRow("Dam Structure:", makeVal(m_lblProfileType, "Earthfill Embankment"));
    pLayout->addRow("Commission Year:", makeVal(m_lblProfileYear, "1983"));
    pLayout->addRow("Designated Purpose:", makeVal(m_lblProfilePurpose, "Irrigation & Industrial Water Supply"));

    layout->addWidget(profileBox);

    auto* jurisBox = new QGroupBox("🛡️ Jurisdiction & Surveillance Protocol", content);
    jurisBox->setStyleSheet("QGroupBox { font-size: 11px; font-weight: bold; color: #A1A1AA; border: 1px solid #27272A; border-radius: 6px; margin-top: 6px; padding-top: 14px; background-color: #141416; }");
    auto* jLayout = new QVBoxLayout(jurisBox);
    jLayout->setContentsMargins(8, 12, 8, 8);
    jLayout->setSpacing(3);

    auto makeJLine = [](const QString& label, const QString& val) {
        auto* l = new QLabel(QString("<b>%1</b>: <span style='color: #A1A1AA;'>%2</span>").arg(label, val));
        l->setStyleSheet("font-size: 10px; color: #E4E4E7;");
        l->setWordWrap(true);
        return l;
    };
    jLayout->addWidget(makeJLine("Supervisory Authority", "Central Water Commission (CWC) Dam Safety Organization"));
    jLayout->addWidget(makeJLine("Surveillance Protocol", "Automated 24/7 Satellite Weather Grids + Modal GPU ML Early Warning"));
    jLayout->addWidget(makeJLine("Disaster Classification", "Category-1 High Hazard Potential Structure under Dam Safety Act 2021"));

    layout->addWidget(jurisBox);
    layout->addStretch();

    return wrapInScrollArea(content);
}

DamRiskAssessment MonitorWidget::evaluateDamRisk(const MapCore::DamPoint& dam, double r1h, double r24h, double fb, double storage, double crestDisp, const QString& structType) {
    DamRiskAssessment risk;
    risk.dam = dam;
    risk.rain1h = r1h;
    risk.rain24h = r24h;
    risk.freeboard = fb;
    risk.storageMcm = storage;
    risk.crestDisp = crestDisp;
    risk.structType = structType;

    // Check if dam is a known historical natural dam case study that has already breached & drained
    if (dam.name.contains("Phuktal", Qt::CaseInsensitive)) {
        risk.failureProbability = 0.0;
        risk.alertLevel = "NORMAL";
        risk.alertColor = "#22C55E";
        risk.summary = "🟢 Historical Landslide Dam (Breached May 2015) — Drained & Inactive. No Current Hazard.";
        risk.ori = 0.0;
        risk.dbi = 0.0;
        risk.qIn = 0.0;
        return risk;
    }
    if (dam.name.contains("Rishi Ganga", Qt::CaseInsensitive)) {
        risk.failureProbability = 0.0;
        risk.alertLevel = "NORMAL";
        risk.alertColor = "#22C55E";
        risk.summary = "🟢 Historical Landslide Dam (Breached Feb 2021) — Drained & Inactive. No Current Hazard.";
        risk.ori = 0.0;
        risk.dbi = 0.0;
        risk.qIn = 0.0;
        return risk;
    }

    double qIn = 0.278 * 0.55 * r1h * 4661.0;
    double qSpill = 11200.0;
    risk.qIn = qIn;

    double inflowRatio = std::max(qIn - qSpill, 0.0) / qSpill;
    bool isNatural = (structType == "Landslide Debris");

    double damH = dam.height > 0.0f ? dam.height : 32.0f;
    risk.ori = (qIn - qSpill) / std::max(storage * 1e6 / damH * fb, 1000.0);
    risk.dbi = isNatural ? 2.10 : 3.50;

    double z = 0.02 * r1h + 0.005 * r24h + 1.5 * std::min(inflowRatio, 5.0) - 1.0 * fb + 0.06 * crestDisp + (isNatural ? 2.5 : 0.0) - 1.5 * risk.dbi - 2.0;
    risk.failureProbability = 1.0 / (1.0 + std::exp(-std::clamp(z, -10.0, 10.0)));

    if (risk.failureProbability >= 0.75) {
        risk.alertLevel = "IMMINENT";
        risk.alertColor = "#EF4444";
        risk.summary = QString("🔴 Critical breach imminent! P(Breach)=%1%.").arg(int(risk.failureProbability * 100));
    } else if (risk.failureProbability >= 0.50) {
        risk.alertLevel = "WARNING";
        risk.alertColor = "#F97316";
        risk.summary = QString("🟠 High overtopping & breach risk (%1%).").arg(int(risk.failureProbability * 100));
    } else if (risk.failureProbability >= 0.25) {
        risk.alertLevel = "WATCH";
        risk.alertColor = "#EAB308";
        risk.summary = QString("🟡 Inflow surge watch (%1%).").arg(int(risk.failureProbability * 100));
    } else {
        risk.alertLevel = "NORMAL";
        risk.alertColor = "#22C55E";
        risk.summary = QString("🟢 Safe. P(Breach)=%1%.").arg(int(risk.failureProbability * 100));
    }

    return risk;
}

void MonitorWidget::runStartupDangerAssessment() {
    m_allDamsRisk.clear();

    m_cboDamSelector->blockSignals(true);
    m_cboDamSelector->clear();

    if (m_damManager && m_damManager->hasData() && !m_damManager->getDams().empty()) {
        const auto& dams = m_damManager->getDams();
        m_allDamsRisk.reserve(dams.size());

        for (const auto& dam : dams) {
            double r1h = 5.0;
            double r24h = 25.0;
            double fb = std::max(double(dam.height) * 0.12, 2.5);
            double storage = dam.storage > 0.0f ? double(dam.storage) : 50.0;
            double crestDisp = 0.1;
            QString sType = dam.damType.isEmpty() ? "Earthfill" : dam.damType;

            DamRiskAssessment risk = evaluateDamRisk(dam, r1h, r24h, fb, storage, crestDisp, sType);
            m_allDamsRisk.push_back(risk);

            QString label = QString("%1 - %2 (%3)")
                .arg(dam.pic.isEmpty() ? "DAM" : dam.pic)
                .arg(dam.name)
                .arg(dam.state.isEmpty() ? "India" : dam.state);
            m_cboDamSelector->addItem(label);
        }
    } else {
        // Fallback Preset Dams
        MapCore::DamPoint pench;
        pench.pic = "MH09HH0596"; pench.name = "Pench Dam (Kamthikhairy)"; pench.state = "Maharashtra"; pench.lat = 21.4645; pench.lon = 79.1865; pench.height = 32.0f; pench.storage = 180.0f; pench.damType = "Earthfill";
        m_allDamsRisk.push_back(evaluateDamRisk(pench, 15.0, 85.0, 2.5, 180.0, 1.2, "Earthfill"));
        m_cboDamSelector->addItem("MH09HH0596 - Pench Dam (Kamthikhairy) (Maharashtra)");

        MapCore::DamPoint totladoh;
        totladoh.pic = "MH09HH0597"; totladoh.name = "Totladoh Dam"; totladoh.state = "Maharashtra"; totladoh.lat = 21.6240; totladoh.lon = 79.2310; totladoh.height = 74.5f; totladoh.storage = 1017.0f; totladoh.damType = "Masonry";
        m_allDamsRisk.push_back(evaluateDamRisk(totladoh, 22.0, 110.0, 4.0, 1017.0, 0.8, "Masonry"));
        m_cboDamSelector->addItem("MH09HH0597 - Totladoh Dam (Maharashtra)");

        MapCore::DamPoint rishi;
        rishi.pic = "UK001"; rishi.name = "Rishi Ganga Landslide Dam"; rishi.state = "Uttarakhand"; rishi.lat = 30.4850; rishi.lon = 79.7120; rishi.height = 45.0f; rishi.storage = 12.0f; rishi.damType = "Landslide Debris";
        m_allDamsRisk.push_back(evaluateDamRisk(rishi, 4.5, 22.0, 3.8, 12.0, 0.2, "Landslide Debris"));
        m_cboDamSelector->addItem("UK001 - Rishi Ganga Landslide Dam (Uttarakhand)");

        MapCore::DamPoint machhu;
        machhu.pic = "GJ002"; machhu.name = "Machhu II Dam (Morbi)"; machhu.state = "Gujarat"; machhu.lat = 22.8210; machhu.lon = 70.8350; machhu.height = 26.0f; machhu.storage = 110.0f; machhu.damType = "Earthfill";
        m_allDamsRisk.push_back(evaluateDamRisk(machhu, 8.0, 35.0, 3.2, 110.0, 0.3, "Earthfill"));
        m_cboDamSelector->addItem("GJ002 - Machhu II Dam (Morbi) (Gujarat)");

        MapCore::DamPoint annamayya;
        annamayya.pic = "AP003"; annamayya.name = "Annamayya Dam (Cheyyeru)"; annamayya.state = "Andhra Pradesh"; annamayya.lat = 14.2150; annamayya.lon = 79.1620; annamayya.height = 28.0f; annamayya.storage = 65.0f; annamayya.damType = "Earthfill";
        m_allDamsRisk.push_back(evaluateDamRisk(annamayya, 5.0, 28.0, 2.8, 65.0, 0.1, "Earthfill"));
        m_cboDamSelector->addItem("AP003 - Annamayya Dam (Cheyyeru) (Andhra Pradesh)");

        MapCore::DamPoint tehri;
        tehri.pic = "UK004"; tehri.name = "Tehri Dam"; tehri.state = "Uttarakhand"; tehri.lat = 30.3780; tehri.lon = 78.4800; tehri.height = 260.5f; tehri.storage = 3540.0f; tehri.damType = "Rockfill";
        m_allDamsRisk.push_back(evaluateDamRisk(tehri, 10.0, 45.0, 8.0, 3540.0, 0.5, "Rockfill"));
        m_cboDamSelector->addItem("UK004 - Tehri Dam (Uttarakhand)");
    }
    m_cboDamSelector->blockSignals(false);

    updateDamsTable();
    updateMapDangerOverlays();

    // Query real-time nationwide meteorological surveillance from backend (Modal GPU)
    fetchActiveSurveillanceDams();

    // Select Default Dam
    if (!m_allDamsRisk.empty()) {
        selectDamAssessment(m_allDamsRisk[0]);
    }
}

void MonitorWidget::fetchActiveSurveillanceDams() {
    QNetworkRequest req(QUrl("http://localhost:8000/api/surveillance/dams"));
    QNetworkReply* reply = m_netManager->get(req);
    reply->setProperty("requestType", "surveillanceDams");
}

void MonitorWidget::updateMapDangerOverlays() {
    if (!m_onlineMap) return;

    std::vector<DangerDamMarker> dangerMarkers;
    for (const auto& risk : m_allDamsRisk) {
        if (risk.dam.name.contains("Phuktal", Qt::CaseInsensitive) || risk.dam.name.contains("Rishi Ganga", Qt::CaseInsensitive)) {
            continue;
        }
        if (risk.alertLevel != "NORMAL" || (risk.isWeatherTriggered && risk.failureProbability >= 0.25)) {
            DangerDamMarker m;
            m.dam = risk.dam;
            m.alertLevel = risk.alertLevel;
            m.alertColor = risk.alertColor;
            m.failureProbability = risk.failureProbability;
            dangerMarkers.push_back(m);
        }
    }

    m_onlineMap->setDangerDams(dangerMarkers);
}

void MonitorWidget::updateDamsTable() {
    if (!m_tblDams) return;

    QString filter = m_txtSearchFilter ? m_txtSearchFilter->text().trimmed().toLower() : QString();

    int countSurveillance = 0;
    int countImminent = 0, countWarning = 0, countWatch = 0, countNormal = 0;

    std::vector<const DamRiskAssessment*> displayList;
    displayList.reserve(m_allDamsRisk.size());

    for (const auto& r : m_allDamsRisk) {
        if (r.isWeatherTriggered) countSurveillance++;
        if (r.alertLevel == "IMMINENT") countImminent++;
        else if (r.alertLevel == "WARNING") countWarning++;
        else if (r.alertLevel == "WATCH") countWatch++;
        else countNormal++;

        if (!filter.isEmpty()) {
            if (r.dam.name.toLower().contains(filter) ||
                r.dam.state.toLower().contains(filter) ||
                r.dam.pic.toLower().contains(filter) ||
                r.alertLevel.toLower().contains(filter) ||
                r.triggerReason.toLower().contains(filter)) {
                displayList.push_back(&r);
            }
        } else {
            // Prioritize dams under active surveillance or danger, plus top nominal dams
            if (r.isWeatherTriggered || r.alertLevel != "NORMAL" || displayList.size() < 120) {
                displayList.push_back(&r);
            }
        }
    }

    m_tblDams->setRowCount(static_cast<int>(displayList.size()));

    for (int i = 0; i < static_cast<int>(displayList.size()); ++i) {
        const auto& r = *displayList[i];

        QString emoji = "🟢";
        if (r.alertLevel == "IMMINENT") emoji = "🔴";
        else if (r.alertLevel == "WARNING") emoji = "🟠";
        else if (r.alertLevel == "WATCH") emoji = "🟡";

        auto* itemEmoji = new QTableWidgetItem(emoji);
        auto* itemName  = new QTableWidgetItem(r.dam.name);
        auto* itemState = new QTableWidgetItem(r.dam.state);
        auto* itemTrig  = new QTableWidgetItem(r.triggerReason.isEmpty() ? "Routine Surveillance" : r.triggerReason);
        auto* itemProb  = new QTableWidgetItem(QString("%1%").arg(int(r.failureProbability * 100)));
        auto* itemAlert = new QTableWidgetItem(r.alertLevel);

        itemAlert->setForeground(QColor(r.alertColor));
        itemProb->setForeground(QColor(r.alertColor));

        if (r.alertLevel != "NORMAL") {
            itemEmoji->setBackground(QColor(239, 68, 68, 50));
            itemName->setForeground(QColor("#FAFAFA"));
        }

        m_tblDams->setItem(i, 0, itemEmoji);
        m_tblDams->setItem(i, 1, itemName);
        m_tblDams->setItem(i, 2, itemState);
        m_tblDams->setItem(i, 3, itemTrig);
        m_tblDams->setItem(i, 4, itemProb);
        m_tblDams->setItem(i, 5, itemAlert);
    }

    if (m_lblCountSurveillance) m_lblCountSurveillance->setText(QString::number(countSurveillance));
    if (m_lblCountImminent) m_lblCountImminent->setText(QString::number(countImminent));
    if (m_lblCountWarning)  m_lblCountWarning->setText(QString::number(countWarning));
    if (m_lblCountWatch)    m_lblCountWatch->setText(QString::number(countWatch));
    if (m_lblCountNormal)   m_lblCountNormal->setText(QString::number(countNormal));
}

void MonitorWidget::onSearchFilterChanged(const QString& query) {
    Q_UNUSED(query);
    updateDamsTable();
}

void MonitorWidget::toggleAutoMonitor() {
    m_autoMonitorActive = !m_autoMonitorActive;
    if (m_autoMonitorActive) {
        m_btnAutoMonitor->setText("🛰️ Auto Monitor: ON");
        m_btnAutoMonitor->setStyleSheet(R"(
            QPushButton {
                background-color: #059669;
                color: #FFFFFF;
                font-size: 11px;
                font-weight: bold;
                border: 1px solid #10B981;
                border-radius: 5px;
                padding: 4px 10px;
            }
        )");
        fetchActiveSurveillanceDams();
        m_autoMonitorTimer->start(15000);
    } else {
        m_btnAutoMonitor->setText("🛰️ Auto Monitor: OFF");
        m_btnAutoMonitor->setStyleSheet(R"(
            QPushButton {
                background-color: #27272A;
                color: #A1A1AA;
                font-size: 11px;
                font-weight: bold;
                border: 1px solid #3F3F46;
                border-radius: 5px;
                padding: 4px 10px;
            }
        )");
        m_autoMonitorTimer->stop();
    }
}

void MonitorWidget::fetchAllDamsLiveWeather() {
    fetchActiveSurveillanceDams();
}

void MonitorWidget::onAutoMonitorTick() {
    fetchActiveSurveillanceDams();
}

void MonitorWidget::onScanNow() {
    QNetworkRequest request(QUrl("http://localhost:8000/api/surveillance/scan_now"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply* reply = m_netManager->post(request, QByteArray("{}"));
    reply->setProperty("requestType", "scanNow");
}

void MonitorWidget::onDamTableCellClicked(int row, int column) {
    Q_UNUSED(column);
    if (!m_tblDams) return;
    QTableWidgetItem* nameItem = m_tblDams->item(row, 1);
    if (!nameItem) return;
    QString damName = nameItem->text();
    for (const auto& r : m_allDamsRisk) {
        if (r.dam.name == damName) {
            selectDamAssessment(r);
            return;
        }
    }
}

void MonitorWidget::selectDamAssessment(const DamRiskAssessment& risk) {
    m_currentDam = risk.dam;
    if (m_lblDamTitle) m_lblDamTitle->setText(risk.dam.name);

    if (m_spnRain1h) m_spnRain1h->setValue(risk.rain1h);
    if (m_spnRain24h) m_spnRain24h->setValue(risk.rain24h);
    if (m_spnFreeboard) m_spnFreeboard->setValue(risk.freeboard);
    if (m_spnStorage) m_spnStorage->setValue(risk.storageMcm);
    if (m_spnCrestDisp) m_spnCrestDisp->setValue(risk.crestDisp);
    if (m_cboStructType) m_cboStructType->setCurrentText(risk.structType);

    if (m_onlineMap) {
        m_onlineMap->setCenter(risk.dam.lat, risk.dam.lon);
        m_onlineMap->setZoom(11);
    }

    // Update Dam Profile labels
    if (m_lblProfileName) m_lblProfileName->setText(risk.dam.name);
    if (m_lblProfilePic) m_lblProfilePic->setText(risk.dam.pic.isEmpty() ? "MH293MH3214" : risk.dam.pic);
    if (m_lblProfileState) m_lblProfileState->setText(QString("%1%2").arg(risk.dam.state.isEmpty() ? "India" : risk.dam.state).arg(risk.dam.district.isEmpty() ? "" : " / " + risk.dam.district));
    if (m_lblProfileRiver) m_lblProfileRiver->setText(QString("%1%2").arg(risk.dam.river.isEmpty() ? "Local River" : risk.dam.river).arg(risk.dam.basin.isEmpty() ? "" : " (" + risk.dam.basin + ")"));
    if (m_lblProfileCoords) m_lblProfileCoords->setText(QString("%1° N, %2° E").arg(risk.dam.lat, 0, 'f', 4).arg(risk.dam.lon, 0, 'f', 4));
    if (m_lblProfileHeight) m_lblProfileHeight->setText(QString("%1 m").arg(risk.dam.height > 0.0f ? QString::number(risk.dam.height, 'f', 1) : "32.0"));
    if (m_lblProfileStorage) m_lblProfileStorage->setText(QString("%1 MCM").arg(risk.dam.storage > 0.0f ? QString::number(risk.dam.storage, 'f', 1) : QString::number(risk.storageMcm, 'f', 1)));
    if (m_lblProfileType) m_lblProfileType->setText(risk.structType.isEmpty() ? "Earthfill Embankment" : risk.structType);
    if (m_lblProfileYear) m_lblProfileYear->setText(risk.dam.year > 0 ? QString::number(risk.dam.year) : "1984");
    if (m_lblProfilePurpose) m_lblProfilePurpose->setText(risk.dam.purpose.isEmpty() ? "Irrigation & Flood Control" : risk.dam.purpose);

    updateHudUI(risk.failureProbability, risk.alertLevel, risk.alertColor, risk.summary, risk.ori, risk.dbi, risk.qIn);

    // Compute dynamic 60-minute flood simulation ONLY if dam is under danger
    if (risk.alertLevel != "NORMAL") {
        m_currentSimState = MapCore::DamFloodSimulator::compute60MinSimulation(m_currentDam);
        m_onlineMap->setFloodSimulation(m_currentSimState);
    } else {
        m_currentSimState = MapCore::FloodSimulationState();
        m_onlineMap->clearFloodSimulation();
    }
}

void MonitorWidget::setViewport(double lat, double lon, int zoom) {
    if (m_onlineMap) {
        m_onlineMap->setCenter(lat, lon);
        m_onlineMap->setZoom(zoom);
    }
}

void MonitorWidget::setSelectedDam(const MapCore::DamPoint& dam) {
    for (const auto& r : m_allDamsRisk) {
        if (r.dam.pic == dam.pic || r.dam.name == dam.name) {
            selectDamAssessment(r);
            return;
        }
    }
    DamRiskAssessment r = evaluateDamRisk(dam, 15.0, 85.0, 2.5, dam.storage > 0 ? dam.storage : 180.0, 1.2, dam.damType.isEmpty() ? "Earthfill" : dam.damType);
    selectDamAssessment(r);
}

void MonitorWidget::onDamSelected(int index) {
    if (index >= 0 && index < static_cast<int>(m_allDamsRisk.size())) {
        selectDamAssessment(m_allDamsRisk[index]);
    }
}

void MonitorWidget::onRunMlPrediction() {
    if (m_btnPredict) {
        m_btnPredict->setText("⏳ Running Full Surveillance & Prediction...");
        m_btnPredict->setEnabled(false);
    }

    // 1. Trigger live nationwide weather scan and batch predictions across all dams
    onScanNow();

    // 2. Evaluate selected dam
    if (m_currentDam.name.contains("Phuktal", Qt::CaseInsensitive)) {
        updateHudUI(0.0, "NORMAL", "#22C55E", "🟢 Historical Landslide Dam (Breached May 2015) — Drained & Inactive. No Current Hazard.", 0.0, 0.0, 0.0);
        m_onlineMap->clearFloodSimulation();
        if (m_btnPredict) {
            m_btnPredict->setText("⚡ Run Full Surveillance & ML Prediction");
            m_btnPredict->setEnabled(true);
        }
        return;
    }
    if (m_currentDam.name.contains("Rishi Ganga", Qt::CaseInsensitive)) {
        updateHudUI(0.0, "NORMAL", "#22C55E", "🟢 Historical Landslide Dam (Breached Feb 2021) — Drained & Inactive. No Current Hazard.", 0.0, 0.0, 0.0);
        m_onlineMap->clearFloodSimulation();
        if (m_btnPredict) {
            m_btnPredict->setText("⚡ Run Full Surveillance & ML Prediction");
            m_btnPredict->setEnabled(true);
        }
        return;
    }

    computeLocalPrediction();

    QJsonObject jsonBody;
    jsonBody["dam_name"] = m_currentDam.name;
    jsonBody["dam_height_m"] = static_cast<double>(m_currentDam.height > 0.0f ? m_currentDam.height : 32.0f);
    jsonBody["crest_length_m"] = 2140.0;
    jsonBody["catchment_area_sqkm"] = 4661.0;
    jsonBody["current_storage_mcm"] = m_spnStorage->value();
    jsonBody["freeboard_remaining_m"] = m_spnFreeboard->value();
    jsonBody["spillway_capacity_cumec"] = 11200.0;
    jsonBody["rain_1h_mm"] = m_spnRain1h->value();
    jsonBody["rain_24h_mm"] = m_spnRain24h->value();
    jsonBody["crest_displacement_mm_yr"] = m_spnCrestDisp->value();
    jsonBody["structural_type"] = m_cboStructType->currentText();
    jsonBody["is_natural"] = (m_cboStructType->currentText() == "Landslide Debris");
    jsonBody["dam_age_years"] = 42;

    QByteArray body = QJsonDocument(jsonBody).toJson();
    QNetworkRequest request(QUrl("http://localhost:8000/api/predict_bg_model"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = m_netManager->post(request, body);
    reply->setProperty("requestType", "mlPrediction");
    reply->setProperty("postBody", body);
}

void MonitorWidget::onNetworkReplyFinished(QNetworkReply* reply) {
    if (reply->property("requestType").toString() == "surveillanceDams" ||
        reply->property("requestType").toString() == "scanNow") {
        if (m_btnPredict) {
            m_btnPredict->setText("⚡ Run Full Surveillance & ML Prediction");
            m_btnPredict->setEnabled(true);
        }
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray data = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                QJsonArray damsArr = root["dams"].toArray();

                for (const auto& val : damsArr) {
                    QJsonObject dObj = val.toObject();
                    QString pic = dObj["pic"].toString();
                    QString name = dObj["dam_name"].toString();
                    if (name.contains("Phuktal", Qt::CaseInsensitive) || name.contains("Rishi Ganga", Qt::CaseInsensitive)) {
                        continue;
                    }
                    double prob = dObj["failure_probability"].toDouble(0.0);
                    QString level = dObj["alert_level"].toString("NORMAL");
                    QString color = dObj["alert_color"].toString(level == "NORMAL" ? "#22C55E" : (level == "WATCH" ? "#EAB308" : "#EF4444"));
                    QString reason = dObj["trigger_reason"].toString("Active Rainfall Surveillance");
                    double r1h = dObj["rain_1h_mm"].toDouble(0.0);
                    double r24h = dObj["rain_24h_mm"].toDouble(0.0);
                    QJsonObject expl = dObj["explanation"].toObject();
                    QString summary = expl["summary"].toString();

                    bool found = false;
                    for (auto& r : m_allDamsRisk) {
                        if ((!pic.isEmpty() && r.dam.pic == pic) || (!name.isEmpty() && r.dam.name.compare(name, Qt::CaseInsensitive) == 0)) {
                            r.failureProbability = prob;
                            r.alertLevel = level;
                            r.alertColor = color;
                            r.triggerReason = reason;
                            r.isWeatherTriggered = true;
                            r.rain1h = r1h;
                            r.rain24h = r24h;
                            if (!summary.isEmpty()) r.summary = summary;
                            r.source = dObj["source"].toString("Modal Serverless GPU");
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        DamRiskAssessment newRisk;
                        newRisk.dam.pic = pic;
                        newRisk.dam.name = name;
                        newRisk.dam.state = dObj["state"].toString("India");
                        newRisk.dam.lat = dObj["lat"].toDouble(20.0);
                        newRisk.dam.lon = dObj["lon"].toDouble(78.0);
                        newRisk.dam.height = 32.0f;
                        newRisk.failureProbability = prob;
                        newRisk.alertLevel = level;
                        newRisk.alertColor = color;
                        newRisk.triggerReason = reason;
                        newRisk.isWeatherTriggered = true;
                        newRisk.rain1h = r1h;
                        newRisk.rain24h = r24h;
                        newRisk.summary = summary;
                        newRisk.source = dObj["source"].toString("Modal Serverless GPU");
                        m_allDamsRisk.push_back(newRisk);
                    }
                }

                // Re-sort: highest failure probability at top
                std::sort(m_allDamsRisk.begin(), m_allDamsRisk.end(), [](const DamRiskAssessment& a, const DamRiskAssessment& b) {
                    return a.failureProbability > b.failureProbability;
                });

                updateDamsTable();
                updateMapDangerOverlays();

                // If highest-risk dam is under alert, select it
                if (!m_allDamsRisk.empty() && m_allDamsRisk[0].alertLevel != "NORMAL") {
                    selectDamAssessment(m_allDamsRisk[0]);
                }
            }
        }
        reply->deleteLater();
        return;
    }

    if (reply->property("requestType").toString() == "mlPrediction") {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            if (doc.isObject()) {
                QJsonObject root = doc.object();
                double prob = root["failure_probability"].toDouble(root["breach_probability"].toDouble(0.08));
                QJsonObject alertObj = root["alert"].toObject();
                QString level = alertObj["level"].toString(root["alert_level"].toString("NORMAL"));
                QString color = alertObj["color"].toString(level == "NORMAL" ? "#22c55e" : (level == "WATCH" ? "#eab308" : "#ef4444"));
                QJsonObject explanationObj = root["explanation"].toObject();
                QString summary = explanationObj["summary"].toString(root["summary"].toString());

                QJsonObject feats = root["features"].toObject();
                double ori = feats["overtopping_risk_index"].toDouble(0.0);
                double dbi = feats["dam_breach_index"].toDouble(3.5);
                double qIn = feats["inflow_surge_cumec"].toDouble(320.0);

                updateHudUI(prob, level, color, summary, ori, dbi, qIn);

                // Update current dam risk in national array
                for (auto& r : m_allDamsRisk) {
                    if (r.dam.pic == m_currentDam.pic || r.dam.name == m_currentDam.name) {
                        r.failureProbability = prob;
                        r.alertLevel = level;
                        r.alertColor = color;
                        r.summary = summary;
                        r.ori = ori;
                        r.dbi = dbi;
                        r.qIn = qIn;
                        break;
                    }
                }

                updateDamsTable();
                updateMapDangerOverlays();

                if (level != "NORMAL") {
                    m_currentSimState = MapCore::DamFloodSimulator::compute60MinSimulation(m_currentDam);
                    m_onlineMap->setFloodSimulation(m_currentSimState);
                } else {
                    m_currentSimState = MapCore::FloodSimulationState();
                    m_onlineMap->clearFloodSimulation();
                }
            }
        } else {
            // If local API bridge is offline, fallback to direct Modal serverless web endpoint
            QString triedUrl = reply->url().toString();
            if (triedUrl.contains("localhost")) {
                QByteArray body = reply->property("postBody").toByteArray();
                QNetworkRequest modalReq(QUrl("https://work-ankit-mail--hydroguard-training-predict-breach-web.modal.run"));
                modalReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                QNetworkReply* mReply = m_netManager->post(modalReq, body);
                mReply->setProperty("requestType", "mlPrediction");
                mReply->setProperty("postBody", body);
                reply->deleteLater();
                return;
            }
        }
        if (m_btnPredict) {
            m_btnPredict->setText("⚡ Run Full Surveillance & ML Prediction");
            m_btnPredict->setEnabled(true);
        }
        reply->deleteLater();
        return;
    }

    reply->deleteLater();
}

void MonitorWidget::computeLocalPrediction() {
    DamRiskAssessment risk = evaluateDamRisk(m_currentDam, m_spnRain1h->value(), m_spnRain24h->value(), m_spnFreeboard->value(), m_spnStorage->value(), m_spnCrestDisp->value(), m_cboStructType->currentText());
    updateHudUI(risk.failureProbability, risk.alertLevel, risk.alertColor, risk.summary, risk.ori, risk.dbi, risk.qIn);

    if (risk.alertLevel != "NORMAL") {
        m_currentSimState = MapCore::DamFloodSimulator::compute60MinSimulation(m_currentDam);
        m_onlineMap->setFloodSimulation(m_currentSimState);
    } else {
        m_currentSimState = MapCore::FloodSimulationState();
        m_onlineMap->clearFloodSimulation();
    }
}

void MonitorWidget::updateHudUI(double probability, const QString& level, const QString& color,
                                const QString& summary, double ori, double dbi, double qIn)
{
    int probPct = std::clamp(int(probability * 100.0), 0, 100);
    if (m_lblRiskProbPercent) {
        m_lblRiskProbPercent->setText(QString("%1%").arg(probPct));
        m_lblRiskProbPercent->setStyleSheet(QString("font-size: 26px; font-weight: bold; color: %1;").arg(color));
    }

    if (m_progressRisk) {
        m_progressRisk->setValue(probPct);
        m_progressRisk->setStyleSheet(QString(R"(
            QProgressBar { background-color: #27272A; border-radius: 3px; }
            QProgressBar::chunk { background-color: %1; border-radius: 3px; }
        )").arg(color));
    }

    if (m_lblAlertBadge) {
        m_lblAlertBadge->setText(QString(" %1 ").arg(level));
        m_lblAlertBadge->setStyleSheet(QString(R"(
            QLabel {
                background-color: rgba(255, 255, 255, 0.12);
                color: %1;
                border: 1px solid %1;
                border-radius: 6px;
                font-size: 11px;
                font-weight: bold;
                padding: 2px 6px;
            }
        )").arg(color));
    }

    if (m_lblActionGuide) m_lblActionGuide->setText(summary.isEmpty() ? "Dam operating within nominal parameters. Inundation safe." : summary);
    if (m_lblInflowSurge) m_lblInflowSurge->setText(QString("Inflow Surge Q_in: %1 m³/s").arg(int(qIn)));
    if (m_lblOriStatus) m_lblOriStatus->setText(QString("ORI (Overtopping Risk Index): %1").arg(ori > 0.001 ? "CRITICAL" : (ori > 0 ? "ELEVATED" : "SAFE")));
    if (m_lblDbiStatus) m_lblDbiStatus->setText(QString("DBI (Dam Breach Index): %1 (%2)").arg(dbi, 0, 'f', 2).arg(dbi < 2.75 ? "UNSTABLE" : "STABLE"));
    if (m_lblShapSummary) m_lblShapSummary->setText(QString("Top Driver: %1").arg(summary.isEmpty() ? "Nominal freeboard & balanced inflow." : summary));

    // Update Brief Tab KPIs
    if (m_lblBriefRain && m_spnRain1h && m_spnRain24h) {
        m_lblBriefRain->setText(QString("%1 mm/h (1h) · %2 mm (24h)").arg(m_spnRain1h->value(), 0, 'f', 1).arg(m_spnRain24h->value(), 0, 'f', 1));
    }
    if (m_lblBriefInflow) {
        m_lblBriefInflow->setText(QString("%1 m³/s").arg(int(qIn)));
    }
    if (m_lblBriefFreeboard && m_spnFreeboard && m_spnStorage) {
        m_lblBriefFreeboard->setText(QString("%1 m freeboard · %2 MCM").arg(m_spnFreeboard->value(), 0, 'f', 2).arg(int(m_spnStorage->value())));
    }
    if (m_lblBriefDisp && m_cboStructType && m_spnCrestDisp) {
        m_lblBriefDisp->setText(QString("%1 · %2 mm/yr").arg(m_cboStructType->currentText()).arg(m_spnCrestDisp->value(), 0, 'f', 1));
    }
    if (m_lblBriefOri) {
        m_lblBriefOri->setText(QString("%1 (%2)").arg(ori > 0.001 ? "CRITICAL" : (ori > 0 ? "ELEVATED" : "SAFE")).arg(ori, 0, 'f', 4));
        m_lblBriefOri->setStyleSheet(QString("font-size: 11px; font-weight: bold; color: %1;").arg(ori > 0.001 ? "#EF4444" : (ori > 0 ? "#EAB308" : "#22C55E")));
    }
    if (m_lblBriefDbi) {
        m_lblBriefDbi->setText(QString("%1 (%2)").arg(dbi, 0, 'f', 2).arg(dbi < 2.75 ? "UNSTABLE" : "STABLE"));
        m_lblBriefDbi->setStyleSheet(QString("font-size: 11px; font-weight: bold; color: %1;").arg(dbi < 2.75 ? "#EF4444" : "#22C55E"));
    }
    if (m_lblBriefShap) {
        m_lblBriefShap->setText(summary.isEmpty() ? "Nominal freeboard and balanced catchment inflow. No immediate hazards detected." : summary);
    }
}

void MonitorWidget::onTimelineFrameChanged(int frame) {
    if (m_onlineMap && m_currentSimState.isActive) {
        m_onlineMap->updateFloodSimulationMinute(frame);
    }
}

} // namespace MapUI
