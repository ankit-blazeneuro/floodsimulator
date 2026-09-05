#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace MapUI {

class NavigationControls : public QWidget {
    Q_OBJECT

private:
    QPushButton* btnZoomIn;
    QPushButton* btnZoomOut;
    QPushButton* btnFitExtent;
    QPushButton* btnResetNorth;
    QPushButton* btnMeasure;
    bool measureActive = false;

public:
    explicit NavigationControls(QWidget* parent = nullptr);

    void setMeasureActive(bool active);
    bool isMeasureActive() const { return measureActive; }

signals:
    void zoomInRequested();
    void zoomOutRequested();
    void fitExtentRequested();
    void resetNorthRequested();
    void measureToggled(bool active);

private:
    void setupUi();
};

class ScaleBar : public QWidget {
    Q_OBJECT

private:
    float currentZoom = 8.0f;
    double centerLat = 26.2;
    int scaleWidthPixels = 100;
    QString scaleText = "50 km";

public:
    explicit ScaleBar(QWidget* parent = nullptr);

    void updateScale(float zoomLevel, double lat, int viewWidth);

protected:
    void paintEvent(QPaintEvent* event) override;
};

} // namespace MapUI
