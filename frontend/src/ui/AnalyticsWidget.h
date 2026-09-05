#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QProgressBar>
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace MapUI {

class AnalyticsWidget : public QWidget {
    Q_OBJECT

public:
    explicit AnalyticsWidget(QWidget* parent = nullptr);
    ~AnalyticsWidget() override = default;

    void fetchMetricsFromModal();

private slots:
    void onRefreshClicked();
    void onNetworkReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* m_netManager = nullptr;

    // Header Widgets
    QLabel* m_lblStatus;
    QLabel* m_lblCloudInfo;
    QPushButton* m_btnRefresh;

    // KPI Cards
    QLabel* m_lblEnsembleAuc;
    QLabel* m_lblBrierLoss;
    QLabel* m_lblXgbAuc;
    QLabel* m_lblLgbmAuc;
    QLabel* m_lblAccuracy;

    // Confusion Matrix Labels
    QLabel* m_lblTN;
    QLabel* m_lblFP;
    QLabel* m_lblFN;
    QLabel* m_lblTP;
    QLabel* m_lblPrecision;
    QLabel* m_lblRecall;
    QLabel* m_lblF1;
    QLabel* m_lblLogLoss;

    // Hyperparameters Labels
    QLabel* m_lblXgbParams;
    QLabel* m_lblLgbmParams;
    QLabel* m_lblDatasetSplit;

    // Feature Importance Table
    QTableWidget* m_tblFeatures;

    void setupUi();
    void updateMetricsFromJson(const QByteArray& jsonData);
    void loadDefaultPresetMetrics();
};

} // namespace MapUI
