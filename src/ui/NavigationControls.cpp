#include "NavigationControls.h"
#include "IconHelper.h"
#include <QPainter>
#include <QGraphicsDropShadowEffect>
#include <cmath>

namespace MapUI {

NavigationControls::NavigationControls(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void NavigationControls::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    QString btnStyle = R"(
        QPushButton {
            background-color: #202124;
            color: #E8EAED;
            border: 1px solid #3C4043;
            border-radius: 20px;
            min-width: 40px;
            max-width: 40px;
            min-height: 40px;
            max-height: 40px;
        }
        QPushButton:hover {
            background-color: #303134;
            border-color: #8AB4F8;
        }
        QPushButton:pressed {
            background-color: #282A2D;
        }
        QPushButton:checked {
            background-color: #4772B3;
            border-color: #5680C2;
        }
    )";

    btnResetNorth = new QPushButton(this);
    btnResetNorth->setIcon(IconHelper::radar(QColor(220, 220, 225), 20));
    btnResetNorth->setIconSize(QSize(20, 20));
    btnResetNorth->setToolTip("Reset Orientation (North Up)");
    btnResetNorth->setStyleSheet(btnStyle);
    auto* shadow0 = new QGraphicsDropShadowEffect(this);
    shadow0->setBlurRadius(10);
    shadow0->setColor(QColor(0, 0, 0, 80));
    shadow0->setOffset(0, 3);
    btnResetNorth->setGraphicsEffect(shadow0);

    btnFitExtent = new QPushButton(this);
    btnFitExtent->setIcon(IconHelper::map(QColor(220, 220, 225), 20));
    btnFitExtent->setIconSize(QSize(20, 20));
    btnFitExtent->setToolTip("Fit Full Extent");
    btnFitExtent->setStyleSheet(btnStyle);
    auto* shadow1 = new QGraphicsDropShadowEffect(this);
    shadow1->setBlurRadius(10);
    shadow1->setColor(QColor(0, 0, 0, 80));
    shadow1->setOffset(0, 3);
    btnFitExtent->setGraphicsEffect(shadow1);

    btnMeasure = new QPushButton(this);
    btnMeasure->setIcon(IconHelper::ruler(QColor(220, 220, 225), 20));
    btnMeasure->setIconSize(QSize(20, 20));
    btnMeasure->setCheckable(true);
    btnMeasure->setToolTip("Measure Distance (Click points on map)");
    btnMeasure->setStyleSheet(btnStyle);
    auto* shadow2 = new QGraphicsDropShadowEffect(this);
    shadow2->setBlurRadius(10);
    shadow2->setColor(QColor(0, 0, 0, 80));
    shadow2->setOffset(0, 3);
    btnMeasure->setGraphicsEffect(shadow2);

    btnZoomIn = new QPushButton(this);
    btnZoomIn->setIcon(IconHelper::zoomIn(QColor(220, 220, 225), 20));
    btnZoomIn->setIconSize(QSize(20, 20));
    btnZoomIn->setToolTip("Zoom In");
    btnZoomIn->setStyleSheet(btnStyle);
    auto* shadow3 = new QGraphicsDropShadowEffect(this);
    shadow3->setBlurRadius(10);
    shadow3->setColor(QColor(0, 0, 0, 80));
    shadow3->setOffset(0, 3);
    btnZoomIn->setGraphicsEffect(shadow3);

    btnZoomOut = new QPushButton(this);
    btnZoomOut->setIcon(IconHelper::zoomOut(QColor(220, 220, 225), 20));
    btnZoomOut->setIconSize(QSize(20, 20));
    btnZoomOut->setToolTip("Zoom Out");
    btnZoomOut->setStyleSheet(btnStyle);
    auto* shadow4 = new QGraphicsDropShadowEffect(this);
    shadow4->setBlurRadius(10);
    shadow4->setColor(QColor(0, 0, 0, 80));
    shadow4->setOffset(0, 3);
    btnZoomOut->setGraphicsEffect(shadow4);

    layout->addWidget(btnResetNorth);
    layout->addWidget(btnFitExtent);
    layout->addWidget(btnMeasure);
    layout->addSpacing(6);
    layout->addWidget(btnZoomIn);
    layout->addWidget(btnZoomOut);

    connect(btnResetNorth, &QPushButton::clicked, this, &NavigationControls::resetNorthRequested);
    connect(btnFitExtent, &QPushButton::clicked, this, &NavigationControls::fitExtentRequested);
    connect(btnZoomIn, &QPushButton::clicked, this, &NavigationControls::zoomInRequested);
    connect(btnZoomOut, &QPushButton::clicked, this, &NavigationControls::zoomOutRequested);
    connect(btnMeasure, &QPushButton::toggled, this, [this](bool checked) {
        measureActive = checked;
        emit measureToggled(checked);
    });
}

void NavigationControls::setMeasureActive(bool active) {
    measureActive = active;
    btnMeasure->setChecked(active);
}

// ScaleBar implementation (Dark Theme)
ScaleBar::ScaleBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(24);
    setFixedWidth(140);
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

void ScaleBar::updateScale(float zoomLevel, double lat, int viewWidth) {
    currentZoom = zoomLevel;
    centerLat = lat;

    double earthCircumference = 40075016.686 * std::cos(lat * 3.1415926535 / 180.0);
    double totalMercatorPixels = 256.0 * std::pow(2.0, static_cast<double>(zoomLevel));
    double metersPerPixel = earthCircumference / totalMercatorPixels;

    double targetMeters = metersPerPixel * 90.0;

    double powerOf10 = std::pow(10.0, std::floor(std::log10(targetMeters)));
    double fraction = targetMeters / powerOf10;

    double roundFactor = 1.0;
    if (fraction >= 5.0) roundFactor = 5.0;
    else if (fraction >= 2.0) roundFactor = 2.0;
    else roundFactor = 1.0;

    double actualMeters = roundFactor * powerOf10;
    scaleWidthPixels = static_cast<int>(actualMeters / metersPerPixel);
    scaleWidthPixels = std::clamp(scaleWidthPixels, 40, 130);

    if (actualMeters >= 1000.0) {
        scaleText = QString("%1 km").arg(actualMeters / 1000.0);
    } else {
        scaleText = QString("%1 m").arg(actualMeters);
    }

    setFixedWidth(scaleWidthPixels + 20);
    update();
}

void ScaleBar::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Dark Background pill
    painter.setBrush(QColor(32, 33, 36, 230));
    painter.setPen(QPen(QColor(60, 64, 67), 1.0));
    painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 4, 4);

    // Scale line (bracket) in crisp light color
    int xStart = 8;
    int xEnd = xStart + scaleWidthPixels;
    int y = height() - 5;

    painter.setPen(QPen(QColor(232, 234, 237), 2.0));
    painter.drawLine(xStart, y, xEnd, y);
    painter.drawLine(xStart, y, xStart, y - 5);
    painter.drawLine(xEnd, y, xEnd, y - 5);

    // Label
    painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
    painter.setPen(QColor(232, 234, 237));
    QRect textRect(xStart, 2, scaleWidthPixels, 12);
    painter.drawText(textRect, Qt::AlignCenter, scaleText);
}

} // namespace MapUI
