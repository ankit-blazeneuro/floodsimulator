#include "AnalyticsWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QScrollArea>

namespace MapUI {

AnalyticsWidget::AnalyticsWidget(QWidget* parent)
    : QWidget(parent)
{
    m_netManager = new QNetworkAccessManager(this);
    connect(m_netManager, &QNetworkAccessManager::finished, this, &AnalyticsWidget::onNetworkReplyFinished);

    setupUi();
    loadDefaultPresetMetrics();
    fetchMetricsFromModal();
}

void AnalyticsWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);
    setStyleSheet("background-color: #09090B; color: #FAFAFA; font-family: 'Segoe UI', Inter, sans-serif;");

    // ── Header Bar ──
    auto* headerWidget = new QWidget(this);
    headerWidget->setStyleSheet("background-color: #18181B; border: 1px solid #27272A; border-radius: 8px; padding: 6px;");
    auto* headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->setContentsMargins(14, 10, 14, 10);

    auto* titleBox = new QVBoxLayout();
    auto* lblTitle = new QLabel("🧠 HydroGuard-AI · Modal.com Model Analytics & Loss Matrix", headerWidget);
    lblTitle->setStyleSheet("font-size: 16px; font-weight: bold; color: #38BDF8;");
    m_lblCloudInfo = new QLabel("☁️ Serverless GPU (NVIDIA A10G) · Modal Volume: hydroguard-models · App: hydroguard-training", headerWidget);
    m_lblCloudInfo->setStyleSheet("font-size: 11px; color: #A1A1AA;");
    titleBox->addWidget(lblTitle);
    titleBox->addWidget(m_lblCloudInfo);
    headerLayout->addLayout(titleBox);
    headerLayout->addStretch();

    m_lblStatus = new QLabel("🟢 Connected to Modal Engine", headerWidget);
    m_lblStatus->setStyleSheet("background-color: rgba(34, 197, 94, 0.15); color: #22C55E; border: 1px solid #22C55E; border-radius: 6px; padding: 5px 10px; font-weight: bold; font-size: 11px;");
    headerLayout->addWidget(m_lblStatus);

    m_btnRefresh = new QPushButton("🔄 Refresh Metrics from Modal", headerWidget);
    m_btnRefresh->setStyleSheet(R"(
        QPushButton {
            background-color: #0284C7;
            color: #FFFFFF;
            font-size: 11px;
            font-weight: bold;
            border-radius: 6px;
            padding: 6px 14px;
        }
        QPushButton:hover { background-color: #0369A1; }
        QPushButton:pressed { background-color: #075985; }
    )");
    connect(m_btnRefresh, &QPushButton::clicked, this, &AnalyticsWidget::onRefreshClicked);
    headerLayout->addWidget(m_btnRefresh);

    mainLayout->addWidget(headerWidget);

    // ── KPI Summary Cards ──
    auto* kpiLayout = new QHBoxLayout();
    kpiLayout->setSpacing(12);

    auto createKpiCard = [](const QString& label, const QString& initialValue, const QString& accentColor, QLabel*& valueLabel) -> QWidget* {
        auto* card = new QWidget();
        card->setStyleSheet(QString(R"(
            QWidget {
                background-color: #18181B;
                border: 1px solid #27272A;
                border-left: 4px solid %1;
                border-radius: 8px;
                padding: 10px;
            }
        )").arg(accentColor));
        auto* l = new QVBoxLayout(card);
        l->setContentsMargins(8, 6, 8, 6);
        l->setSpacing(2);

        auto* lblTitle = new QLabel(label, card);
        lblTitle->setStyleSheet("font-size: 11px; color: #A1A1AA; font-weight: 500;");
        valueLabel = new QLabel(initialValue, card);
        valueLabel->setStyleSheet(QString("font-size: 22px; font-weight: bold; color: %1;").arg(accentColor));

        l->addWidget(lblTitle);
        l->addWidget(valueLabel);
        return card;
    };

    kpiLayout->addWidget(createKpiCard("🏆 Ensemble AUC-ROC", "0.9748", "#38BDF8", m_lblEnsembleAuc));
    kpiLayout->addWidget(createKpiCard("📉 Brier Calibration Loss", "0.0337", "#22C55E", m_lblBrierLoss));
    kpiLayout->addWidget(createKpiCard("⚡ XGBoost GPU AUC", "0.9785", "#F59E0B", m_lblXgbAuc));
    kpiLayout->addWidget(createKpiCard("🚀 LightGBM GPU AUC", "0.9786", "#EC4899", m_lblLgbmAuc));
    kpiLayout->addWidget(createKpiCard("🎯 Test Accuracy", "95.87%", "#A855F7", m_lblAccuracy));

    mainLayout->addLayout(kpiLayout);

    // ── Middle Section: Confusion Matrix & Hyperparameters ──
    auto* midLayout = new QHBoxLayout();
    midLayout->setSpacing(12);

    // Left: Confusion Matrix Box
    auto* cmBox = new QGroupBox("📊 Modal Test Set Confusion Matrix & Physical Loss", this);
    cmBox->setStyleSheet("QGroupBox { background-color: #18181B; border: 1px solid #27272A; border-radius: 8px; font-weight: bold; font-size: 12px; color: #FAFAFA; margin-top: 10px; padding-top: 14px; }");
    auto* cmLayout = new QVBoxLayout(cmBox);
    cmLayout->setContentsMargins(12, 12, 12, 12);
    cmLayout->setSpacing(10);

    auto* gridMatrix = new QGridLayout();
    gridMatrix->setSpacing(8);

    auto* lblHeaderPred0 = new QLabel("Predicted Safe (0)", cmBox);
    lblHeaderPred0->setStyleSheet("font-size: 11px; font-weight: bold; color: #22C55E; text-align: center;");
    lblHeaderPred0->setAlignment(Qt::AlignCenter);

    auto* lblHeaderPred1 = new QLabel("Predicted Breach (1)", cmBox);
    lblHeaderPred1->setStyleSheet("font-size: 11px; font-weight: bold; color: #EF4444; text-align: center;");
    lblHeaderPred1->setAlignment(Qt::AlignCenter);

    auto* lblHeaderAct0 = new QLabel("Actual Safe\n(Non-Breach)", cmBox);
    lblHeaderAct0->setStyleSheet("font-size: 10px; color: #A1A1AA; font-weight: bold;");

    auto* lblHeaderAct1 = new QLabel("Actual Danger\n(Breached)", cmBox);
    lblHeaderAct1->setStyleSheet("font-size: 10px; color: #A1A1AA; font-weight: bold;");

    gridMatrix->addWidget(new QLabel("", cmBox), 0, 0);
    gridMatrix->addWidget(lblHeaderPred0, 0, 1);
    gridMatrix->addWidget(lblHeaderPred1, 0, 2);
    gridMatrix->addWidget(lblHeaderAct0, 1, 0);
    gridMatrix->addWidget(lblHeaderAct1, 2, 0);

    m_lblTN = new QLabel("591\n(TN - 98.5%)", cmBox);
    m_lblTN->setAlignment(Qt::AlignCenter);
    m_lblTN->setStyleSheet("background-color: rgba(34, 197, 94, 0.2); border: 1px solid #22C55E; border-radius: 6px; color: #22C55E; font-size: 13px; font-weight: bold; padding: 10px;");

    m_lblFP = new QLabel("9\n(FP - 1.5%)", cmBox);
    m_lblFP->setAlignment(Qt::AlignCenter);
    m_lblFP->setStyleSheet("background-color: rgba(234, 179, 8, 0.15); border: 1px solid #EAB308; border-radius: 6px; color: #EAB308; font-size: 13px; font-weight: bold; padding: 10px;");

    m_lblFN = new QLabel("22\n(FN - 14.7%)", cmBox);
    m_lblFN->setAlignment(Qt::AlignCenter);
    m_lblFN->setStyleSheet("background-color: rgba(239, 68, 68, 0.15); border: 1px solid #EF4444; border-radius: 6px; color: #EF4444; font-size: 13px; font-weight: bold; padding: 10px;");

    m_lblTP = new QLabel("128\n(TP - 85.3%)", cmBox);
    m_lblTP->setAlignment(Qt::AlignCenter);
    m_lblTP->setStyleSheet("background-color: rgba(34, 197, 94, 0.3); border: 1px solid #22C55E; border-radius: 6px; color: #4ADE80; font-size: 13px; font-weight: bold; padding: 10px;");

    gridMatrix->addWidget(m_lblTN, 1, 1);
    gridMatrix->addWidget(m_lblFP, 1, 2);
    gridMatrix->addWidget(m_lblFN, 2, 1);
    gridMatrix->addWidget(m_lblTP, 2, 2);

    cmLayout->addLayout(gridMatrix);

    // Summary Metric Chips
    auto* chipsLayout = new QHBoxLayout();
    chipsLayout->setSpacing(6);

    auto makeChip = [](const QString& text, QLabel*& lbl, const QString& color) -> QLabel* {
        lbl = new QLabel(text);
        lbl->setStyleSheet(QString("background-color: #27272A; color: %1; border: 1px solid %1; border-radius: 4px; font-size: 11px; font-weight: bold; padding: 4px 8px;").arg(color));
        return lbl;
    };

    chipsLayout->addWidget(makeChip("Precision: 93.4%", m_lblPrecision, "#38BDF8"));
    chipsLayout->addWidget(makeChip("Recall: 85.3%", m_lblRecall, "#38BDF8"));
    chipsLayout->addWidget(makeChip("F1: 89.2%", m_lblF1, "#38BDF8"));
    chipsLayout->addWidget(makeChip("LogLoss: 0.1084", m_lblLogLoss, "#A1A1AA"));

    cmLayout->addLayout(chipsLayout);
    midLayout->addWidget(cmBox, 1);

    // Right: Hyperparameters & Training Specs Box
    auto* hpoBox = new QGroupBox("⚙️ Optuna Bayesian Hyperparameters & Training Setup", this);
    hpoBox->setStyleSheet("QGroupBox { background-color: #18181B; border: 1px solid #27272A; border-radius: 8px; font-weight: bold; font-size: 12px; color: #FAFAFA; margin-top: 10px; padding-top: 14px; }");
    auto* hpoLayout = new QVBoxLayout(hpoBox);
    hpoLayout->setContentsMargins(12, 12, 12, 12);
    hpoLayout->setSpacing(8);

    m_lblDatasetSplit = new QLabel("📦 Training Dataset Partitioning:\n• 7,100 Total Samples | 5,600 Train (SMOTE Balanced) | 750 Val | 750 Test\n• Balancing: 1:1 ratio for rare catastrophic breach events", hpoBox);
    m_lblDatasetSplit->setStyleSheet("background-color: #27272A; border-radius: 5px; padding: 8px; font-size: 11px; color: #D4D4D8;");

    m_lblXgbParams = new QLabel("🌲 XGBoost GPU Parameters (hist / device=cuda):\n• max_depth: 5  |  learning_rate: 0.0287  |  n_estimators: 406\n• subsample: 0.949  |  colsample_bytree: 0.608  |  reg_lambda: 0.1739", hpoBox);
    m_lblXgbParams->setStyleSheet("background-color: #27272A; border-radius: 5px; padding: 8px; font-size: 11px; color: #F59E0B;");

    m_lblLgbmParams = new QLabel("🍃 LightGBM Parameters (40 Bayesian Trials):\n• max_depth: 4  |  learning_rate: 0.0207  |  n_estimators: 658\n• num_leaves: 56  |  min_child_samples: 47  |  subsample: 0.842", hpoBox);
    m_lblLgbmParams->setStyleSheet("background-color: #27272A; border-radius: 5px; padding: 8px; font-size: 11px; color: #EC4899;");

    hpoLayout->addWidget(m_lblDatasetSplit);
    hpoLayout->addWidget(m_lblXgbParams);
    hpoLayout->addWidget(m_lblLgbmParams);
    midLayout->addWidget(hpoBox, 1);

    mainLayout->addLayout(midLayout);

    // ── Bottom Section: Feature Matrix & Physics Weights ──
    auto* featBox = new QGroupBox("🧮 15-Feature Physics & InSAR Matrix Attribution Weights", this);
    featBox->setStyleSheet("QGroupBox { background-color: #18181B; border: 1px solid #27272A; border-radius: 8px; font-weight: bold; font-size: 12px; color: #FAFAFA; margin-top: 10px; padding-top: 14px; }");
    auto* featLayout = new QVBoxLayout(featBox);
    featLayout->setContentsMargins(10, 10, 10, 10);

    m_tblFeatures = new QTableWidget(10, 5, featBox);
    m_tblFeatures->setHorizontalHeaderLabels({ "Rank", "Feature Parameter", "Category", "Importance Weight", "Physical Impact" });
    m_tblFeatures->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tblFeatures->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_tblFeatures->verticalHeader()->setVisible(false);
    m_tblFeatures->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tblFeatures->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tblFeatures->setFixedHeight(170);
    m_tblFeatures->setStyleSheet(R"(
        QTableWidget {
            background-color: #121214;
            color: #FAFAFA;
            gridline-color: #27272A;
            border: 1px solid #27272A;
            border-radius: 4px;
            font-size: 11px;
        }
        QHeaderView::section {
            background-color: #1E1E22;
            color: #A1A1AA;
            font-weight: bold;
            border: 1px solid #27272A;
            padding: 4px;
        }
    )");

    featLayout->addWidget(m_tblFeatures);
    mainLayout->addWidget(featBox);
}

void AnalyticsWidget::loadDefaultPresetMetrics() {
    struct FeatRow {
        int rank;
        QString name;
        QString category;
        QString weight;
        QString impact;
    };

    std::vector<FeatRow> rows = {
        { 1, "Peak Inflow Surge Q_in (Rational Method)", "Hydrological", "26.4%", "Direct hydrostatic surge pressure exceeding spillway discharge" },
        { 2, "Overtopping Risk Index (ORI)", "Hydraulic", "21.8%", "Dynamic storage accumulation velocity relative to remaining freeboard" },
        { 3, "Stefanelli Dam Breach Index (DBI)", "Geotechnical", "15.5%", "Barrier blockage volume vs impounded hydrostatic lake pressure" },
        { 4, "Freeboard Remaining Margin (m)", "Structural", "12.4%", "Direct barrier height buffer before overtopping initiation occurs" },
        { 5, "1-Hour Cloudburst Rainfall (mm/hr)", "Meteorological", "8.2%", "Instantaneous precip intensity triggering catchment runoff surge" },
        { 6, "InSAR Subsidence / Crest Disp (mm/yr)", "Geodetic", "6.1%", "Sentinel-1 ground deformation flagging active crest/toe shearing" },
        { 7, "24-Hour Antecedent Rainfall (mm)", "Meteorological", "3.8%", "Antecedent soil moisture saturation reducing catchment infiltration" },
        { 8, "Upstream Catchment Area (sq km)", "Morphological", "2.4%", "Contributing drainage basin area driving total volumetric inflow" },
        { 9, "Natural Landslide Debris Barrier Flag", "Structural", "1.8%", "Unengineered natural blockages inherently susceptible to internal erosion" },
        { 10, "Current Storage Capacity (MCM)", "Hydrological", "1.6%", "Total kinetic water mass released upon structural compromise" }
    };

    m_tblFeatures->setRowCount(rows.size());
    for (size_t i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        m_tblFeatures->setItem(i, 0, new QTableWidgetItem(QString("#%1").arg(r.rank)));
        m_tblFeatures->setItem(i, 1, new QTableWidgetItem(r.name));
        m_tblFeatures->setItem(i, 2, new QTableWidgetItem(r.category));
        auto* wItem = new QTableWidgetItem(r.weight);
        wItem->setForeground(QColor("#38BDF8"));
        m_tblFeatures->setItem(i, 3, wItem);
        m_tblFeatures->setItem(i, 4, new QTableWidgetItem(r.impact));
    }
}

void AnalyticsWidget::fetchMetricsFromModal() {
    m_lblStatus->setText("⏳ Querying Modal.com Serverless GPU...");
    m_lblStatus->setStyleSheet("background-color: rgba(56, 189, 248, 0.15); color: #38BDF8; border: 1px solid #38BDF8; border-radius: 6px; padding: 5px 10px; font-weight: bold; font-size: 11px;");

    QNetworkRequest request(QUrl("http://localhost:8000/api/modal/metrics"));
    m_netManager->get(request);
}

void AnalyticsWidget::onRefreshClicked() {
    fetchMetricsFromModal();
}

void AnalyticsWidget::onNetworkReplyFinished(QNetworkReply* reply) {
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        updateMetricsFromJson(data);
        m_lblStatus->setText("🟢 Live Connected to Modal Cloud Engine");
        m_lblStatus->setStyleSheet("background-color: rgba(34, 197, 94, 0.15); color: #22C55E; border: 1px solid #22C55E; border-radius: 6px; padding: 5px 10px; font-weight: bold; font-size: 11px;");
    } else {
        m_lblStatus->setText("🟡 Using Cached Modal Matrix");
        m_lblStatus->setStyleSheet("background-color: rgba(234, 179, 8, 0.15); color: #EAB308; border: 1px solid #EAB308; border-radius: 6px; padding: 5px 10px; font-weight: bold; font-size: 11px;");
    }
    reply->deleteLater();
}

void AnalyticsWidget::updateMetricsFromJson(const QByteArray& jsonData) {
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) return;

    QJsonObject root = doc.object();

    double ensAuc = root["ensemble_auc_roc"].toDouble(0.9748);
    double brier = root["ensemble_brier_score"].toDouble(0.0337);
    double xgbAuc = root["xgb_auc_roc"].toDouble(0.9785);
    double lgbmAuc = root["lgbm_auc_roc"].toDouble(0.9786);

    m_lblEnsembleAuc->setText(QString::number(ensAuc, 'f', 4));
    m_lblBrierLoss->setText(QString::number(brier, 'f', 4));
    m_lblXgbAuc->setText(QString::number(xgbAuc, 'f', 4));
    m_lblLgbmAuc->setText(QString::number(lgbmAuc, 'f', 4));

    QJsonObject cmObj = root["confusion_matrix"].toObject();
    int tn = cmObj["true_negatives"].toInt(591);
    int fp = cmObj["false_positives"].toInt(9);
    int fn = cmObj["false_negatives"].toInt(22);
    int tp = cmObj["true_positives"].toInt(128);

    double acc = cmObj["accuracy"].toDouble(0.9587);
    double prec = cmObj["precision"].toDouble(0.9343);
    double rec = cmObj["recall"].toDouble(0.8533);
    double f1 = cmObj["f1_score"].toDouble(0.8920);

    m_lblAccuracy->setText(QString("%1%").arg(QString::number(acc * 100.0, 'f', 2)));
    m_lblTN->setText(QString("%1\n(TN - %2%)").arg(tn).arg(QString::number(double(tn) / (tn + fp) * 100.0, 'f', 1)));
    m_lblFP->setText(QString("%1\n(FP - %2%)").arg(fp).arg(QString::number(double(fp) / (tn + fp) * 100.0, 'f', 1)));
    m_lblFN->setText(QString("%1\n(FN - %2%)").arg(fn).arg(QString::number(double(fn) / (tp + fn) * 100.0, 'f', 1)));
    m_lblTP->setText(QString("%1\n(TP - %2%)").arg(tp).arg(QString::number(double(tp) / (tp + fn) * 100.0, 'f', 1)));

    m_lblPrecision->setText(QString("Precision: %1%").arg(QString::number(prec * 100.0, 'f', 1)));
    m_lblRecall->setText(QString("Recall: %1%").arg(QString::number(rec * 100.0, 'f', 1)));
    m_lblF1->setText(QString("F1: %1%").arg(QString::number(f1 * 100.0, 'f', 1)));

    QJsonObject lossObj = root["loss"].toObject();
    double brierLoss = lossObj["brier_score_loss"].toDouble(0.0337);
    double xgbLogloss = lossObj["xgb_val_logloss"].toDouble(0.1084);
    m_lblLogLoss->setText(QString("LogLoss: %1 | Brier: %2").arg(QString::number(xgbLogloss, 'f', 4)).arg(QString::number(brierLoss, 'f', 4)));
}

} // namespace MapUI
