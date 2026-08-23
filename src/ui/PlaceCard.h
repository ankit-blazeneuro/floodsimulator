#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../core/SpatialIndex.h"

namespace MapUI {

class PlaceCard : public QWidget {
    Q_OBJECT

private:
    QLabel* lblIcon;
    QLabel* lblTitle;
    QLabel* lblCategory;
    QLabel* lblCoords;
    QLabel* lblDistance;
    QTableWidget* tableTags;
    QPushButton* btnZoomIn;
    QPushButton* btnMeasure;
    QPushButton* btnCopyCoords;
    QPushButton* btnClose;

    MapCore::FeatureInfo currentFeature;

public:
    explicit PlaceCard(QWidget* parent = nullptr);

    void setFeature(const MapCore::FeatureInfo& info);
    const MapCore::FeatureInfo& getFeature() const { return currentFeature; }

signals:
    void zoomInRequested(MapCore::Point2D pos);
    void measureFromRequested(MapCore::Point2D pos);
    void cardClosed();

private:
    void setupUi();
    QString formatDMS(double val, bool isLat) const;
};

} // namespace MapUI
