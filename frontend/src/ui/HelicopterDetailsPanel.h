#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QListWidget>
#include <vector>
#include "../core/HelicopterTrackerManager.h"

namespace MapUI {

class HelicopterDetailsPanel : public QWidget {
    Q_OBJECT

public:
    explicit HelicopterDetailsPanel(QWidget* parent = nullptr);
    ~HelicopterDetailsPanel() override = default;

    void setHelicopter(const MapCore::HelicopterTrack& heli);
    void updateHelicopterList(const std::vector<MapCore::HelicopterTrack>& list);
    void setLiveStatus(const QString& statusText);

signals:
    void helicopterSelectedFromList(const QString& hex);
    void centerMapRequested(double lat, double lon);
    void closeRequested();

private slots:
    void onListItemClicked(QListWidgetItem* item);
    void onCenterClicked();

private:
    void setupUi();
    QWidget* createMetricCard(const QString& icon, const QString& title, QLabel*& valLabel, const QString& unit);

    QLabel* lblHeaderStatus;
    QPushButton* btnClose;

    QLabel* lblCallsign;
    QLabel* lblModel;
    QLabel* lblOperator;
    QLabel* lblEmergencyBadge;

    QLabel* lblAltitudeVal;
    QLabel* lblSpeedVal;
    QLabel* lblHeadingVal;
    QLabel* lblVerticalRateVal;
    QLabel* lblCoordinatesVal;
    QLabel* lblTransponderVal;

    QPushButton* btnCenterMap;
    QListWidget* fleetListWidget;

    MapCore::HelicopterTrack currentHeli;
    std::vector<MapCore::HelicopterTrack> currentFleet;
};

} // namespace MapUI
