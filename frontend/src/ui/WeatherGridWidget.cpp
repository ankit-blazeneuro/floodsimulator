#include "WeatherGridWidget.h"
#include "IconHelper.h"
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QDateTime>
#include <cmath>
#include <algorithm>

namespace MapUI {

// ============================================================================
// WeatherGridCellWidget Implementation
// ============================================================================

WeatherGridCellWidget::WeatherGridCellWidget(WeatherGridMetric metric, WeatherGridWidget* parentDashboard, QWidget* parent)
    : QWidget(parent)
    , metricType(metric)
    , dashboard(parentDashboard) {
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);

    setupHeaderAndFooter();

    // Connect to central TileCacheManager
    connect(&MapCore::TileCacheManager::instance(), &MapCore::TileCacheManager::tileLoaded, this,
            [this](int provider, int zoom, int x, int y) {
                Q_UNUSED(provider);
                Q_UNUSED(zoom);
                Q_UNUSED(x);
                Q_UNUSED(y);
                update();
            });
}

void WeatherGridCellWidget::setupHeaderAndFooter() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 6);
    mainLayout->setSpacing(0);

    // 1. Top Header Bar (Transparent, floating fixed over the map)
    headerBar = new QWidget(this);
    headerBar->setFixedHeight(28);
    headerBar->setStyleSheet("background-color: transparent; border: none;");

    auto* headerLayout = new QHBoxLayout(headerBar);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(6);

    lblTitle = new QLabel(headerBar);
    lblTitle->setStyleSheet(R"(
        QLabel {
            font-family: 'Segoe UI', Inter, -apple-system, sans-serif;
            font-size: 11px;
            font-weight: bold;
            color: #F4F4F5;
            background-color: rgba(20, 20, 24, 0.85);
            border: 1px solid rgba(63, 63, 70, 0.60);
            border-radius: 5px;
            padding: 3px 8px;
        }
    )");

    btnMaximize = new QPushButton("⛶", headerBar);
    btnMaximize->setFixedSize(24, 24);
    btnMaximize->setToolTip("Maximize / Restore Grid");
    btnMaximize->setStyleSheet(R"(
        QPushButton {
            background-color: rgba(20, 20, 24, 0.85);
            color: #A1A1AA;
            border: 1px solid rgba(63, 63, 70, 0.60);
            border-radius: 5px;
            font-size: 12px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #3F3F46;
            color: #FFFFFF;
            border-color: #34D399;
        }
    )");
    connect(btnMaximize, &QPushButton::clicked, this, [this]() {
        emit toggleMaximizeRequested(this);
    });

    QString titleText;
    switch (metricType) {
        case WeatherGridMetric::TemperatureHeatmap:
            titleText = "🌡️ Thermal & Temperature Heatmap";
            break;
        case WeatherGridMetric::PrecipitationRadar:
            titleText = "🌧️ Precipitation & Doppler Radar";
            break;
        case WeatherGridMetric::WindStreamlines:
            titleText = "💨 Wind Velocity & Aerodynamics";
            break;
        case WeatherGridMetric::CloudSatellite:
            titleText = "☁️ Cloud Cover & Satellite Layer";
            break;
        case WeatherGridMetric::RelativeHumidity:
            titleText = "💧 Relative Humidity & Moisture";
            break;
        case WeatherGridMetric::SevereRiskComposite:
            titleText = "⚡ Multi-Hazard Composite Overview";
            break;
    }

    lblTitle->setText(titleText);

    headerLayout->addWidget(lblTitle, 0, Qt::AlignLeft | Qt::AlignVCenter);
    headerLayout->addStretch(1);
    headerLayout->addWidget(btnMaximize, 0, Qt::AlignRight | Qt::AlignVCenter);

    mainLayout->addWidget(headerBar, 0);
    mainLayout->addStretch(1); // Central map area painted directly

    // 2. Bottom Footer Bar (Transparent container with standalone simple dot)
    footerBar = new QWidget(this);
    footerBar->setFixedHeight(22);
    footerBar->setStyleSheet("background-color: transparent; border: none;");

    auto* footerLayout = new QHBoxLayout(footerBar);
    footerLayout->setContentsMargins(0, 0, 0, 0);
    footerLayout->setSpacing(6);

    statusDot = new StatusDotWidget(footerBar);

    lblLegendText = new QLabel(footerBar);
    lblLegendText->setStyleSheet(R"(
        QLabel {
            font-size: 9px;
            color: #D4D4D8;
            font-weight: 500;
            background-color: rgba(18, 18, 22, 0.80);
            border: 1px solid rgba(63, 63, 70, 0.50);
            border-radius: 4px;
            padding: 2px 6px;
        }
    )");

    footerLayout->addWidget(statusDot, 0, Qt::AlignLeft | Qt::AlignVCenter);
    footerLayout->addWidget(lblLegendText, 0, Qt::AlignLeft | Qt::AlignVCenter);

    mainLayout->addWidget(footerBar, 0, Qt::AlignLeft | Qt::AlignBottom);

    updateMetricBadge();
}

void WeatherGridCellWidget::setForecast(const MapCore::WeatherForecastData& forecast, int hourIndex) {
    currentForecast = forecast;
    currentHourIndex = hourIndex;
    updateMetricBadge();
    update();
}

void WeatherGridCellWidget::setHourIndex(int hourIndex) {
    currentHourIndex = hourIndex;
    updateMetricBadge();
    update();
}

void WeatherGridCellWidget::setCenter(double lat, double lon) {
    centerLat = std::clamp(lat, -85.0511, 85.0511);
    centerLon = std::clamp(lon, -180.0, 180.0);
    update();
}

void WeatherGridCellWidget::setZoom(int zoom) {
    zoomLevel = std::clamp(zoom, 2, 18);
    update();
}

void WeatherGridCellWidget::setTileProvider(MapCore::OnlineTileProvider provider) {
    currentProvider = provider;
    update();
}

void WeatherGridCellWidget::setAnimPhase(int phase) {
    animPhase = phase;
    update();
}

void WeatherGridCellWidget::setMaximized(bool max) {
    isMaximizedState = max;
    btnMaximize->setText(max ? "🗗" : "⛶");
    btnMaximize->setToolTip(max ? "Restore 6-Grid View" : "Maximize to Fullscreen");
}

void WeatherGridCellWidget::setSelected(bool sel) {
    if (isSelectedState != sel) {
        isSelectedState = sel;
        update();
    }
}

void WeatherGridCellWidget::renderCenterReticle(QPainter& p) {
    double cx = width() / 2.0;
    double cy = height() / 2.0;
    double arm = 11.0;     // Arm length
    double gap = 3.5;      // Center gap

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);

    // 1. Drop shadow for high contrast across dark and bright map tiles
    QPen shadowPen(QColor(0, 0, 0, 200), 3.0, Qt::SolidLine, Qt::RoundCap);
    p.setPen(shadowPen);

    // Horizontal shadow
    p.drawLine(QPointF(cx - arm, cy), QPointF(cx - gap, cy));
    p.drawLine(QPointF(cx + gap, cy), QPointF(cx + arm, cy));

    // Vertical shadow
    p.drawLine(QPointF(cx, cy - arm), QPointF(cx, cy - gap));
    p.drawLine(QPointF(cx, cy + gap), QPointF(cx, cy + arm));

    // Center dot shadow
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 200));
    p.drawEllipse(QPointF(cx, cy), 2.2, 2.2);

    // 2. Foreground crisp plus sign (White cross with Emerald accent center dot)
    QPen reticlePen(QColor(255, 255, 255, 240), 1.5, Qt::SolidLine, Qt::RoundCap);
    p.setPen(reticlePen);

    // Horizontal arms
    p.drawLine(QPointF(cx - arm, cy), QPointF(cx - gap, cy));
    p.drawLine(QPointF(cx + gap, cy), QPointF(cx + arm, cy));

    // Vertical arms
    p.drawLine(QPointF(cx, cy - arm), QPointF(cx, cy - gap));
    p.drawLine(QPointF(cx, cy + gap), QPointF(cx, cy + arm));

    // Center Emerald core dot
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(52, 211, 153));
    p.drawEllipse(QPointF(cx, cy), 1.5, 1.5);

    p.restore();
}

void WeatherGridCellWidget::updateMetricBadge() {
    bool connected = currentForecast.isValid && !currentForecast.hourly.empty();
    if (statusDot) {
        statusDot->setConnected(connected);
    }

    if (!connected) {
        lblLegendText->setText("");
        lblLegendText->hide();
        return;
    }

    lblLegendText->show();

    const auto* hw = currentForecast.getHour(currentHourIndex);
    if (!hw) {
        if (statusDot) statusDot->setConnected(false);
        lblLegendText->setText("");
        lblLegendText->hide();
        return;
    }

    switch (metricType) {
        case WeatherGridMetric::TemperatureHeatmap:
            lblLegendText->setText(QString("[-10°C Freezing ❄️ | 0°C | 15°C Mild | 25°C Warm | 35°C Hot 🔥 | 45°C Extreme] · Current: %1°C")
                                  .arg(hw->temperatureC, 0, 'f', 1));
            break;
        case WeatherGridMetric::PrecipitationRadar:
            lblLegendText->setText(QString("[0.0mm Dry | 1.0mm Light 🌦️ | 5.0mm Mod 🌧️ | 15.0mm Heavy ⛈️ | 30mm+ Torrential] · %1 mm/h")
                                  .arg(hw->precipitationMm, 0, 'f', 1));
            break;
        case WeatherGridMetric::WindStreamlines:
            lblLegendText->setText(QString("[<5 km/h Calm | 15 km/h Breeze 🍃 | 30 km/h Strong | 50 km/h Gale 💨 | 75+ Storm] · %1 km/h")
                                  .arg(hw->windSpeedKmh, 0, 'f', 1));
            break;
        case WeatherGridMetric::CloudSatellite:
            lblLegendText->setText(QString("[Low: %1% · Mid: %2% · High: %3%] · Total: %4%")
                                   .arg(qRound(hw->cloudCoverLow))
                                   .arg(qRound(hw->cloudCoverMid))
                                   .arg(qRound(hw->cloudCoverHigh))
                                   .arg(qRound(hw->cloudCoverPct)));
            break;
        case WeatherGridMetric::RelativeHumidity: {
            double dewPoint = hw->temperatureC - ((100.0 - hw->relativeHumidity) / 5.0);
            lblLegendText->setText(QString("[<30% Dry 🏜️ | 30-60% Optimal 🌿 | 60-80% Humid 💦 | >80% Fog 🌫️] · %1% RH (Dew Point: %2°C)")
                                  .arg(qRound(hw->relativeHumidity))
                                  .arg(dewPoint, 0, 'f', 1));
            break;
        }
        case WeatherGridMetric::SevereRiskComposite: {
            lblLegendText->setText("[0-25 Normal 🟢 | 26-50 Moderate 🟡 | 51-75 Severe Watch 🟠 | 76-100 Emergency 🔴]");
            break;
        }
    }
}

// Map Coordinate Mathematics
double WeatherGridCellWidget::lonToTileX(double lon, int zoom) const {
    return MapCore::TileCacheManager::lonToTileX(lon, zoom);
}

double WeatherGridCellWidget::latToTileY(double lat, int zoom) const {
    return MapCore::TileCacheManager::latToTileY(lat, zoom);
}

double WeatherGridCellWidget::tileXToLon(double x, int zoom) const {
    return MapCore::TileCacheManager::tileXToLon(x, zoom);
}

double WeatherGridCellWidget::tileYToLat(double y, int zoom) const {
    return MapCore::TileCacheManager::tileYToLat(y, zoom);
}

QPointF WeatherGridCellWidget::geoToScreen(double lat, double lon) const {
    double cx = width() / 2.0;
    double cy = height() / 2.0;

    double centerTileX = lonToTileX(centerLon, zoomLevel);
    double centerTileY = latToTileY(centerLat, zoomLevel);

    double targetTileX = lonToTileX(lon, zoomLevel);
    double targetTileY = latToTileY(lat, zoomLevel);

    double sx = cx + (targetTileX - centerTileX) * 256.0;
    double sy = cy + (targetTileY - centerTileY) * 256.0;
    return QPointF(sx, sy);
}

double WeatherGridCellWidget::screenToLat(double screenY) const {
    double cy = height() / 2.0;
    double centerTileY = latToTileY(centerLat, zoomLevel);
    double targetTileY = centerTileY + (screenY - cy) / 256.0;
    return tileYToLat(targetTileY, zoomLevel);
}

double WeatherGridCellWidget::screenToLon(double screenX) const {
    double cx = width() / 2.0;
    double centerTileX = lonToTileX(centerLon, zoomLevel);
    double targetTileX = centerTileX + (screenX - cx) / 256.0;
    return tileXToLon(targetTileX, zoomLevel);
}

// Paint Event
void WeatherGridCellWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();
    double cx = w / 2.0;
    double cy = h / 2.0;

    bool isDark = (currentProvider == MapCore::OnlineTileProvider::OpenStreetMap_Dark);

    // Rounded corner clip path for slight curve (radius 6.0px)
    QPainterPath clipPath;
    clipPath.addRoundedRect(QRectF(0, 0, w, h), 6.0, 6.0);
    painter.setClipPath(clipPath);

    // Canvas background adapting to theme
    painter.fillRect(rect(), isDark ? QColor(20, 20, 24) : QColor(245, 245, 248));

    // 1. Draw Shared OpenStreetMap Tiles (Engine calls OSM once, duplicates 6 times)
    double centerTileX = lonToTileX(centerLon, zoomLevel);
    double centerTileY = latToTileY(centerLat, zoomLevel);

    double offsetX = cx - (centerTileX - std::floor(centerTileX)) * 256.0;
    double offsetY = cy - (centerTileY - std::floor(centerTileY)) * 256.0;

    int centerTileXInt = static_cast<int>(std::floor(centerTileX));
    int centerTileYInt = static_cast<int>(std::floor(centerTileY));

    double maxDiag = std::sqrt(w * w + h * h) / 2.0;
    int tilesRadius = static_cast<int>(std::ceil(maxDiag / 256.0)) + 1;

    for (int dy = -tilesRadius; dy <= tilesRadius; ++dy) {
        for (int dx = -tilesRadius; dx <= tilesRadius; ++dx) {
            int tileX = centerTileXInt + dx;
            int tileY = centerTileYInt + dy;

            int drawX = static_cast<int>(offsetX + dx * 256.0);
            int drawY = static_cast<int>(offsetY + dy * 256.0);

            QPixmap* tile = MapCore::TileCacheManager::instance().getTile(currentProvider, zoomLevel, tileX, tileY);
            if (tile && !tile->isNull()) {
                painter.drawPixmap(drawX, drawY, 256, 256, *tile);
            } else {
                QRect tileRect(drawX, drawY, 256, 256);
                painter.fillRect(tileRect, isDark ? QColor(28, 28, 32) : QColor(232, 232, 236));
                painter.setPen(QPen(isDark ? QColor(42, 42, 48) : QColor(215, 215, 220), 1));
                painter.drawRect(tileRect);
            }
        }
    }

    // 2. Render Specialized Weather Layer Overlays for this specific grid
    switch (metricType) {
        case WeatherGridMetric::TemperatureHeatmap:
            renderTemperatureHeatmap(painter);
            break;
        case WeatherGridMetric::PrecipitationRadar:
            renderPrecipitationRadar(painter);
            break;
        case WeatherGridMetric::WindStreamlines:
            renderWindStreamlines(painter);
            break;
        case WeatherGridMetric::CloudSatellite:
            renderCloudSatellite(painter);
            break;
        case WeatherGridMetric::RelativeHumidity:
            renderRelativeHumidity(painter);
            break;
        case WeatherGridMetric::SevereRiskComposite:
            renderSevereRiskComposite(painter);
            break;
    }

    // 3. Render Center Reticle / Plus Sign on Selected Grid Cell (Same as simulation screen)
    if (isSelectedState) {
        renderCenterReticle(painter);
    }

    // Disable clipping to draw outer border cleanly
    painter.setClipping(false);

    // 4. Grid Cell Outer Border (Slight curve and gray color for selected grid)
    if (isSelectedState) {
        painter.setPen(QPen(QColor(161, 161, 170, 240), 1.5)); // Zinc-400 crisp gray
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75), 6.0, 6.0);
    } else {
        painter.setPen(QPen(QColor(39, 39, 42, 200), 1.0)); // Zinc-800 dark gray
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 6.0, 6.0);
    }
}

// 1. Temperature Heatmap Overlay
void WeatherGridCellWidget::renderTemperatureHeatmap(QPainter& p) {
    if (!currentForecast.isValid) return;
    const auto* hw = currentForecast.getHour(currentHourIndex);
    if (!hw) return;

    QPointF centerPt = geoToScreen(currentForecast.latitude, currentForecast.longitude);
    double temp = hw->temperatureC;

    // Thermal Color Ramp
    QColor heatColor;
    if (temp < 0.0) heatColor = QColor(59, 130, 246, 90);       // Freezing Blue
    else if (temp < 15.0) heatColor = QColor(6, 182, 212, 100);  // Cyan
    else if (temp < 25.0) heatColor = QColor(34, 197, 94, 100);  // Green
    else if (temp < 32.0) heatColor = QColor(234, 179, 8, 110);  // Amber
    else if (temp < 38.0) heatColor = QColor(249, 115, 22, 125); // Orange
    else heatColor = QColor(225, 29, 72, 140);                   // Crimson Red

    // Radial multi-stop thermal heat wash
    double heatRadius = std::min(width(), height()) * 0.75;
    QRadialGradient grad(centerPt, heatRadius);
    grad.setColorAt(0.0, heatColor);
    grad.setColorAt(0.5, QColor(heatColor.red(), heatColor.green(), heatColor.blue(), heatColor.alpha() / 2));
    grad.setColorAt(1.0, QColor(heatColor.red(), heatColor.green(), heatColor.blue(), 0));

    p.fillRect(rect(), grad);

    // Isotherm concentric contour lines
    p.setPen(QPen(QColor(254, 202, 202, 120), 1.2, Qt::DashLine));
    for (int r = 1; r <= 3; ++r) {
        double radius = r * (heatRadius / 3.2);
        p.drawEllipse(centerPt, radius, radius);
    }

    // Central pulsing thermal beacon
    double pulse = std::fmod(animPhase * 0.6, 14.0);
    p.setPen(QPen(QColor(239, 68, 68, std::max(0, 220 - static_cast<int>(pulse * 15))), 1.5));
    p.setBrush(QColor(239, 68, 68, 45));
    p.drawEllipse(centerPt, 6.0 + pulse, 6.0 + pulse);

    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(QColor(239, 68, 68));
    p.drawEllipse(centerPt, 5.0, 5.0);

    // Floating temperature readout badge
    QString label = QString("🌡️ %1°C (Max: %2°C)").arg(temp, 0, 'f', 1).arg(temp + 3.5, 0, 'f', 1);
    QFont font("Segoe UI", 8, QFont::Bold);
    p.setFont(font);
    QFontMetricsF fm(font);
    QRectF badgeRect(centerPt.x() - fm.horizontalAdvance(label) / 2.0 - 6, centerPt.y() - 28, fm.horizontalAdvance(label) + 12, fm.height() + 4);
    p.setBrush(QColor(24, 24, 27, 230));
    p.setPen(QPen(QColor(239, 68, 68), 1.0));
    p.drawRoundedRect(badgeRect, 4, 4);
    p.setPen(QColor(244, 244, 245));
    p.drawText(badgeRect, Qt::AlignCenter, label);
}

// 2. Precipitation Doppler Radar Overlay
void WeatherGridCellWidget::renderPrecipitationRadar(QPainter& p) {
    if (!currentForecast.isValid) return;
    const auto* hw = currentForecast.getHour(currentHourIndex);
    if (!hw) return;

    QPointF centerPt = geoToScreen(currentForecast.latitude, currentForecast.longitude);
    double precip = hw->precipitationMm;

    // Concentric Doppler Radar Range Rings
    p.setPen(QPen(QColor(56, 189, 248, 60), 1.0, Qt::DashLine));
    for (int ring = 1; ring <= 4; ++ring) {
        double r = ring * 45.0;
        p.drawEllipse(centerPt, r, r);
    }

    // Rotating 360° Doppler Radar Sweep Beam
    double sweepAngle = std::fmod(animPhase * 3.5, 360.0);
    double sweepRad = sweepAngle * M_PI / 180.0;
    double beamLength = std::max(width(), height()) * 0.6;
    QPointF beamEnd(centerPt.x() + beamLength * std::cos(sweepRad), centerPt.y() + beamLength * std::sin(sweepRad));

    QPainterPath sweepWedge;
    sweepWedge.moveTo(centerPt);
    sweepWedge.arcTo(centerPt.x() - beamLength, centerPt.y() - beamLength, beamLength * 2, beamLength * 2, -sweepAngle, 40.0);
    sweepWedge.closeSubpath();

    QLinearGradient wedgeGrad(centerPt, beamEnd);
    wedgeGrad.setColorAt(0.0, QColor(56, 189, 248, 80));
    wedgeGrad.setColorAt(1.0, QColor(56, 189, 248, 0));
    p.fillPath(sweepWedge, wedgeGrad);

    p.setPen(QPen(QColor(56, 189, 248, 200), 1.6));
    p.drawLine(centerPt, beamEnd);

    // Rain Echo Reflectivity Gradient & Rain Streaks
    if (precip > 0.0) {
        int alpha = std::clamp(static_cast<int>(precip * 25.0) + 30, 30, 110);
        QColor rainColor = precip > 5.0 ? QColor(225, 29, 72, alpha) : (precip > 1.5 ? QColor(2, 132, 199, alpha) : QColor(52, 211, 153, alpha));
        p.fillRect(rect(), rainColor);

        // Animated falling rain vector streaks
        int numStreaks = std::clamp(static_cast<int>(precip * 20.0) + 20, 20, 70);
        p.setPen(QPen(QColor(186, 230, 253, 140), 1.2, Qt::SolidLine, Qt::RoundCap));
        for (int s = 0; s < numStreaks; ++s) {
            double sx = std::fmod((s * 37.0 + animPhase * 3.0), static_cast<double>(width()));
            double sy = std::fmod((s * 61.0 + animPhase * 10.0), static_cast<double>(height()));
            p.drawLine(QPointF(sx, sy), QPointF(sx + 3.0, sy + 12.0));
        }
    }

    // Center Radar Station Beacon
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(QColor(56, 189, 248));
    p.drawEllipse(centerPt, 5.0, 5.0);

    // Radar HUD Tag
    QString label = QString("🌧️ %1 mm/h · Radar Echo").arg(precip, 0, 'f', 1);
    QFont font("Segoe UI", 8, QFont::Bold);
    p.setFont(font);
    QFontMetricsF fm(font);
    QRectF badgeRect(centerPt.x() - fm.horizontalAdvance(label) / 2.0 - 6, centerPt.y() - 28, fm.horizontalAdvance(label) + 12, fm.height() + 4);
    p.setBrush(QColor(24, 24, 27, 230));
    p.setPen(QPen(QColor(56, 189, 248), 1.0));
    p.drawRoundedRect(badgeRect, 4, 4);
    p.setPen(QColor(244, 244, 245));
    p.drawText(badgeRect, Qt::AlignCenter, label);
}

// 3. Wind Velocity & Aerodynamics Overlay
void WeatherGridCellWidget::renderWindStreamlines(QPainter& p) {
    if (!currentForecast.isValid) return;
    const auto* hw = currentForecast.getHour(currentHourIndex);
    if (!hw) return;

    double windSpeed = hw->windSpeedKmh;

    // Moving aerodynamic streamline flow vectors
    int numLines = 9;
    double speedFactor = std::clamp(windSpeed * 0.6 + 2.0, 2.0, 30.0);
    p.setPen(QPen(QColor(251, 191, 36, 130), 1.4, Qt::DashLine, Qt::RoundCap));

    for (int i = 0; i < numLines; ++i) {
        double y = (height() / (numLines + 1.0)) * (i + 1.0);
        double offset = std::fmod((animPhase * speedFactor + i * 85.0), static_cast<double>(width() + 100.0)) - 50.0;
        p.drawLine(QPointF(offset, y), QPointF(offset + 70.0 + windSpeed * 2.0, y + 3.0));

        // Arrow head
        QPolygonF arrow;
        arrow << QPointF(offset + 70.0 + windSpeed * 2.0, y + 3.0)
              << QPointF(offset + 64.0 + windSpeed * 2.0, y - 2.0)
              << QPointF(offset + 64.0 + windSpeed * 2.0, y + 8.0);
        p.setBrush(QColor(251, 191, 36, 160));
        p.drawPolygon(arrow);
    }

    // Compass Rose in Top-Right
    QPointF compassCenter(width() - 45.0, 58.0);
    p.setBrush(QColor(24, 24, 27, 220));
    p.setPen(QPen(QColor(251, 191, 36), 1.2));
    p.drawEllipse(compassCenter, 18.0, 18.0);

    // Cardinal marks
    p.setPen(QPen(QColor(212, 212, 216), 1.0));
    QFont cFont("Segoe UI", 7, QFont::Bold);
    p.setFont(cFont);
    p.drawText(QRectF(compassCenter.x() - 10, compassCenter.y() - 17, 20, 10), Qt::AlignCenter, "N");

    // Wind direction needle
    double needleAngle = (animPhase * 0.5);
    double nRad = needleAngle * M_PI / 180.0;
    p.setPen(QPen(QColor(248, 113, 113), 2.0, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(compassCenter, QPointF(compassCenter.x() + 12.0 * std::sin(nRad), compassCenter.y() - 12.0 * std::cos(nRad)));

    // Target pinpoint
    QPointF centerPt = geoToScreen(currentForecast.latitude, currentForecast.longitude);
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(QColor(251, 191, 36));
    p.drawEllipse(centerPt, 5.0, 5.0);

    QString label = QString("💨 %1 km/h · WSW").arg(windSpeed, 0, 'f', 1);
    QFont font("Segoe UI", 8, QFont::Bold);
    p.setFont(font);
    QFontMetricsF fm(font);
    QRectF badgeRect(centerPt.x() - fm.horizontalAdvance(label) / 2.0 - 6, centerPt.y() - 28, fm.horizontalAdvance(label) + 12, fm.height() + 4);
    p.setBrush(QColor(24, 24, 27, 230));
    p.setPen(QPen(QColor(251, 191, 36), 1.0));
    p.drawRoundedRect(badgeRect, 4, 4);
    p.setPen(QColor(244, 244, 245));
    p.drawText(badgeRect, Qt::AlignCenter, label);
}

// 4. Cloud Cover & Satellite Overlay
void WeatherGridCellWidget::renderCloudSatellite(QPainter& p) {
    if (!currentForecast.isValid) return;
    const auto* hw = currentForecast.getHour(currentHourIndex);
    if (!hw) return;

    double cloudPct = hw->cloudCoverPct;

    // Atmospheric Cloud Opacity Mask
    if (cloudPct > 2.0) {
        int alpha = std::clamp(static_cast<int>((cloudPct / 100.0) * 110.0), 15, 110);
        p.fillRect(rect(), QColor(241, 245, 249, alpha));

        // Drifting cloud fractals
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, alpha / 2));
        for (int c = 0; c < 5; ++c) {
            double cx = std::fmod((c * 90.0 + animPhase * 1.5), static_cast<double>(width() + 100.0)) - 50.0;
            double cy = (c * 65.0 + 30.0);
            p.drawEllipse(QPointF(cx, cy), 50.0, 30.0);
        }
    }

    QPointF centerPt = geoToScreen(currentForecast.latitude, currentForecast.longitude);
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(QColor(203, 213, 225));
    p.drawEllipse(centerPt, 5.0, 5.0);

    QString label = QString("☁️ %1% Cloud Cover").arg(qRound(cloudPct));
    QFont font("Segoe UI", 8, QFont::Bold);
    p.setFont(font);
    QFontMetricsF fm(font);
    QRectF badgeRect(centerPt.x() - fm.horizontalAdvance(label) / 2.0 - 6, centerPt.y() - 28, fm.horizontalAdvance(label) + 12, fm.height() + 4);
    p.setBrush(QColor(24, 24, 27, 230));
    p.setPen(QPen(QColor(148, 163, 184), 1.0));
    p.drawRoundedRect(badgeRect, 4, 4);
    p.setPen(QColor(244, 244, 245));
    p.drawText(badgeRect, Qt::AlignCenter, label);
}

// 5. Relative Humidity & Moisture Overlay
void WeatherGridCellWidget::renderRelativeHumidity(QPainter& p) {
    if (!currentForecast.isValid) return;
    const auto* hw = currentForecast.getHour(currentHourIndex);
    if (!hw) return;

    double rh = hw->relativeHumidity;
    int alpha = std::clamp(static_cast<int>((rh / 100.0) * 105.0), 20, 105);

    // Deep Teal Moisture Gradient Wash
    QColor moistureColor = rh > 80.0 ? QColor(13, 148, 136, alpha) : (rh > 50.0 ? QColor(20, 184, 166, alpha) : QColor(45, 212, 191, alpha));
    p.fillRect(rect(), moistureColor);

    // Floating moisture vapor particles
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(153, 246, 228, 60));
    for (int vp = 0; vp < 12; ++vp) {
        double vx = std::fmod((vp * 43.0 + animPhase * 0.8), static_cast<double>(width()));
        double vy = std::fmod((vp * 57.0 - animPhase * 1.2 + height() * 2), static_cast<double>(height()));
        p.drawEllipse(QPointF(vx, vy), 6.0, 6.0);
    }

    QPointF centerPt = geoToScreen(currentForecast.latitude, currentForecast.longitude);
    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(QColor(45, 212, 191));
    p.drawEllipse(centerPt, 5.0, 5.0);

    double dewPoint = hw->temperatureC - ((100.0 - rh) / 5.0);
    QString label = QString("💧 %1% RH · Dew %2°C").arg(qRound(rh)).arg(dewPoint, 0, 'f', 1);
    QFont font("Segoe UI", 8, QFont::Bold);
    p.setFont(font);
    QFontMetricsF fm(font);
    QRectF badgeRect(centerPt.x() - fm.horizontalAdvance(label) / 2.0 - 6, centerPt.y() - 28, fm.horizontalAdvance(label) + 12, fm.height() + 4);
    p.setBrush(QColor(24, 24, 27, 230));
    p.setPen(QPen(QColor(45, 212, 191), 1.0));
    p.drawRoundedRect(badgeRect, 4, 4);
    p.setPen(QColor(244, 244, 245));
    p.drawText(badgeRect, Qt::AlignCenter, label);
}

// 6. Severe Risk Composite Overview Overlay
void WeatherGridCellWidget::renderSevereRiskComposite(QPainter& p) {
    if (!currentForecast.isValid) return;
    const auto* hw = currentForecast.getHour(currentHourIndex);
    if (!hw) return;

    double riskScore = std::clamp((hw->precipitationMm / 20.0) * 40.0 + (hw->windSpeedKmh / 50.0) * 30.0 + (hw->cloudCoverPct / 100.0) * 15.0 + (hw->relativeHumidity / 100.0) * 15.0, 0.0, 100.0);
    QPointF centerPt = geoToScreen(currentForecast.latitude, currentForecast.longitude);

    // Multi-ring pulsing emergency hazard beacon
    double pulse = std::fmod(animPhase * 0.8, 20.0);
    QColor beaconColor = riskScore > 65.0 ? QColor(239, 68, 68) : (riskScore > 35.0 ? QColor(245, 158, 11) : QColor(34, 197, 94));

    p.setPen(QPen(QColor(beaconColor.red(), beaconColor.green(), beaconColor.blue(), std::max(0, 200 - static_cast<int>(pulse * 10))), 1.8));
    p.setBrush(QColor(beaconColor.red(), beaconColor.green(), beaconColor.blue(), 30));
    p.drawEllipse(centerPt, 8.0 + pulse, 8.0 + pulse);
    p.drawEllipse(centerPt, 16.0 + pulse, 16.0 + pulse);

    p.setPen(QPen(Qt::white, 1.5));
    p.setBrush(beaconColor);
    p.drawEllipse(centerPt, 6.0, 6.0);

    // 24-Hour Mini Forecast Trend Sparkline in Bottom Area
    if (!currentForecast.hourly.empty()) {
        int nHours = std::min(24, static_cast<int>(currentForecast.hourly.size()));
        double sparkWidth = width() - 30.0;
        double sparkLeft = 15.0;
        double sparkBottom = height() - 32.0;
        double sparkHeight = 28.0;

        QRectF sparkRect(sparkLeft - 4, sparkBottom - sparkHeight - 4, sparkWidth + 8, sparkHeight + 8);
        p.fillRect(sparkRect, QColor(20, 20, 24, 210));
        p.setPen(QPen(QColor(39, 39, 42), 1));
        p.drawRoundedRect(sparkRect, 4, 4);

        QPainterPath sparkPath;
        for (int h = 0; h < nHours; ++h) {
            double x = sparkLeft + (sparkWidth / (nHours - 1.0)) * h;
            double t = currentForecast.hourly[h].temperatureC;
            double normY = std::clamp((t - 15.0) / 25.0, 0.0, 1.0);
            double y = sparkBottom - normY * sparkHeight;

            if (h == 0) sparkPath.moveTo(x, y);
            else sparkPath.lineTo(x, y);
        }

        p.setPen(QPen(QColor(192, 132, 252), 1.8));
        p.setBrush(Qt::NoBrush);
        p.drawPath(sparkPath);

        // Current hour indicator dot on sparkline
        int activeIdx = std::clamp(currentHourIndex, 0, nHours - 1);
        double activeX = sparkLeft + (sparkWidth / (nHours - 1.0)) * activeIdx;
        double activeT = currentForecast.hourly[activeIdx].temperatureC;
        double activeY = sparkBottom - std::clamp((activeT - 15.0) / 25.0, 0.0, 1.0) * sparkHeight;
        p.setPen(QPen(Qt::white, 1.2));
        p.setBrush(QColor(236, 72, 153));
        p.drawEllipse(QPointF(activeX, activeY), 3.5, 3.5);
    }

    // Readout Badge
    QString label = QString("⚡ Risk: %1/100 · %2").arg(qRound(riskScore)).arg(currentForecast.locationName);
    QFont font("Segoe UI", 8, QFont::Bold);
    p.setFont(font);
    QFontMetricsF fm(font);
    QRectF badgeRect(centerPt.x() - fm.horizontalAdvance(label) / 2.0 - 6, centerPt.y() - 28, fm.horizontalAdvance(label) + 12, fm.height() + 4);
    p.setBrush(QColor(24, 24, 27, 230));
    p.setPen(QPen(QColor(192, 132, 252), 1.0));
    p.drawRoundedRect(badgeRect, 4, 4);
    p.setPen(QColor(244, 244, 245));
    p.drawText(badgeRect, Qt::AlignCenter, label);
}

// Mouse and Wheel Events
void WeatherGridCellWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        lastMousePos = event->pos();
        pressMousePos = event->pos();
        emit cellSelected(this);
    }
}

void WeatherGridCellWidget::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging) {
        QPoint delta = event->pos() - lastMousePos;
        lastMousePos = event->pos();

        double dx = delta.x();
        double dy = delta.y();

        double centerTileX = lonToTileX(centerLon, zoomLevel) - dx / 256.0;
        double centerTileY = latToTileY(centerLat, zoomLevel) - dy / 256.0;

        centerLon = tileXToLon(centerTileX, zoomLevel);
        centerLat = tileYToLat(centerTileY, zoomLevel);

        emit viewportChanged(centerLat, centerLon, zoomLevel, this);
        update();
    }
}

void WeatherGridCellWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        if ((event->pos() - pressMousePos).manhattanLength() < 5) {
            // Click to inspect location
            double clickLat = screenToLat(event->pos().y());
            double clickLon = screenToLon(event->pos().x());
            emit locationClicked(clickLat, clickLon);
        }
    }
}

void WeatherGridCellWidget::wheelEvent(QWheelEvent* event) {
    int numDegrees = event->angleDelta().y() / 8;
    int numSteps = numDegrees / 15;

    if (numSteps != 0) {
        int newZoom = std::clamp(zoomLevel + (numSteps > 0 ? 1 : -1), 2, 18);
        if (newZoom != zoomLevel) {
            zoomLevel = newZoom;
            emit viewportChanged(centerLat, centerLon, zoomLevel, this);
            update();
        }
    }
    event->accept();
}

void WeatherGridCellWidget::mouseDoubleClickEvent(QMouseEvent* /*event*/) {
    emit toggleMaximizeRequested(this);
}

// ============================================================================
// WeatherGridWidget (Main Workspace) Implementation
// ============================================================================

WeatherGridWidget::WeatherGridWidget(QWidget* parent)
    : QWidget(parent)
    , forecastManager(new MapCore::WeatherForecastManager(this)) {
    setObjectName("weatherGridWidget");
    setStyleSheet("QWidget#weatherGridWidget { background-color: #121215; }");

    setupUi();

    // 30 FPS Animation Timer for live fluid dynamics (radar sweep, rain, streamlines)
    animTimer = new QTimer(this);
    animTimer->setInterval(33);
    connect(animTimer, &QTimer::timeout, this, [this]() {
        animPhase = (animPhase + 1) % 10000;
        for (auto* cell : gridCells) {
            cell->setAnimPhase(animPhase);
        }
    });
    animTimer->start();

    // Wire WeatherForecastManager
    connect(forecastManager, &MapCore::WeatherForecastManager::forecastUpdated, this, &WeatherGridWidget::onForecastReceived);
    connect(forecastManager, &MapCore::WeatherForecastManager::fetchFailed, this, [](const QString& err) {
        qWarning() << "[WeatherGridWidget] Forecast fetch failed:" << err;
    });

    // Initial weather fetch matching simulation screen default (India Region: 22.0° N, 79.0° E, Zoom 5)
    fetchWeatherForLocation(22.0, 79.0, "India Region");
}

void WeatherGridWidget::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Horizontal Splitter: [ Left: Weather Sidebar | Right: Vertical Splitter ]
    hSplitter = new QSplitter(Qt::Horizontal, this);
    hSplitter->setHandleWidth(2);
    hSplitter->setStyleSheet(R"(
        QSplitter::handle:horizontal {
            background-color: #27272A;
            width: 2px;
        }
        QSplitter::handle:horizontal:hover {
            background-color: #34D399;
        }
        QSplitter::handle:vertical {
            background-color: #27272A;
            height: 2px;
        }
        QSplitter::handle:vertical:hover {
            background-color: #34D399;
        }
    )");

    // Left Resizable Sidebar
    sidebar = new WeatherHudWidget(hSplitter);

    // 2. Vertical Splitter: [ Top: Grid Container | Bottom: Timeline Widget ]
    vSplitter = new QSplitter(Qt::Vertical, hSplitter);
    vSplitter->setHandleWidth(2);

    setupGridCells();

    // Bottom Weather Timeline (Same capabilities and Blender scrubber as simulation)
    weatherTimeline = new TimelineWidget(TimelineMode::WeatherForecast, vSplitter);

    vSplitter->addWidget(gridContainer);
    vSplitter->addWidget(weatherTimeline);
    vSplitter->setCollapsible(0, false);
    vSplitter->setCollapsible(1, true);

    hSplitter->addWidget(sidebar);
    hSplitter->addWidget(vSplitter);
    hSplitter->setCollapsible(0, false);
    hSplitter->setCollapsible(1, false);

    mainLayout->addWidget(hSplitter, 1);

    // Wire Sidebar Signals
    connect(sidebar, &WeatherHudWidget::locationRequested, this, [this](double lat, double lon, const QString& name) {
        sharedLat = lat;
        sharedLon = lon;
        for (auto* cell : gridCells) {
            cell->setCenter(lat, lon);
        }
        fetchWeatherForLocation(lat, lon, name);
    });

    connect(sidebar, &WeatherHudWidget::refreshRequested, this, [this]() {
        fetchWeatherForLocation(sharedLat, sharedLon, currentForecast.locationName);
    });

    connect(sidebar, &WeatherHudWidget::fitAssamRequested, this, &WeatherGridWidget::resetToAssam);
    connect(sidebar, &WeatherHudWidget::fitIndiaRequested, this, &WeatherGridWidget::resetToIndia);
    connect(sidebar, &WeatherHudWidget::syncToggled, this, &WeatherGridWidget::setSyncEnabled);
    connect(sidebar, &WeatherHudWidget::tileProviderChanged, this, &WeatherGridWidget::setTileProvider);

    connect(sidebar, &WeatherHudWidget::hourSelected, this, [this](int hour) {
        if (weatherTimeline) {
            weatherTimeline->setCurrentFrame(hour);
        }
        setTimeHour(hour);
    });

    // Wire Timeline Signal
    connect(weatherTimeline, &TimelineWidget::frameChanged, this, [this](int hour, const QString& timeCode) {
        Q_UNUSED(timeCode);
        setTimeHour(hour);
    });
}

void WeatherGridWidget::setupGridCells() {
    gridContainer = new QWidget(vSplitter);
    gridContainer->setStyleSheet("background-color: transparent;");

    gridLayout = new QGridLayout(gridContainer);
    gridLayout->setContentsMargins(6, 6, 6, 6);
    gridLayout->setSpacing(6);

    // 3 equal columns for 1:1 width ratio with the sidebar
    gridLayout->setColumnStretch(0, 1);
    gridLayout->setColumnStretch(1, 1);
    gridLayout->setColumnStretch(2, 1);
    gridLayout->setRowStretch(0, 1);
    gridLayout->setRowStretch(1, 1);

    // Instantiate the 6 distinct weather metric cells
    WeatherGridMetric metrics[6] = {
        WeatherGridMetric::TemperatureHeatmap,
        WeatherGridMetric::PrecipitationRadar,
        WeatherGridMetric::WindStreamlines,
        WeatherGridMetric::CloudSatellite,
        WeatherGridMetric::RelativeHumidity,
        WeatherGridMetric::SevereRiskComposite
    };

    for (int i = 0; i < 6; ++i) {
        auto* cell = new WeatherGridCellWidget(metrics[i], this, gridContainer);
        cell->setCenter(sharedLat, sharedLon);
        cell->setZoom(sharedZoom);
        cell->setTileProvider(sharedProvider);

        connect(cell, &WeatherGridCellWidget::viewportChanged, this, &WeatherGridWidget::onCellViewportChanged);
        connect(cell, &WeatherGridCellWidget::locationClicked, this, &WeatherGridWidget::onCellLocationClicked);
        connect(cell, &WeatherGridCellWidget::toggleMaximizeRequested, this, &WeatherGridWidget::onToggleMaximizeCell);
        connect(cell, &WeatherGridCellWidget::cellSelected, this, [this](WeatherGridCellWidget* c) {
            for (auto* other : gridCells) {
                other->setSelected(other == c);
            }
        });

        gridCells.push_back(cell);

        int row = i / 3;
        int col = i % 3;
        gridLayout->addWidget(cell, row, col);
    }

    if (!gridCells.empty()) {
        gridCells[0]->setSelected(true);
    }
}

void WeatherGridWidget::applyDefaultSplitterSizes() {
    int totalW = width();
    if (totalW > 100) {
        // Default sidebar width is exactly the same as one of the 3 map cards (totalW / 4)
        int sidebarW = std::max(280, totalW / 4);
        int rightW = std::max(400, totalW - sidebarW);
        hSplitter->setSizes({ sidebarW, rightW });
    }

    int totalH = height();
    if (totalH > 100) {
        int timelineH = 170;
        int gridH = std::max(200, totalH - timelineH);
        vSplitter->setSizes({ gridH, timelineH });
    }
}

void WeatherGridWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!initialSplitterSizesSet && width() > 100) {
        initialSplitterSizesSet = true;
        applyDefaultSplitterSizes();
    }
}

void WeatherGridWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initialSplitterSizesSet && width() > 100) {
        initialSplitterSizesSet = true;
        applyDefaultSplitterSizes();
    }
}

void WeatherGridWidget::fetchWeatherForLocation(double lat, double lon, const QString& locationName) {
    forecastManager->fetchForecast(lat, lon, locationName);
}

void WeatherGridWidget::setTimeHour(int hourIndex) {
    currentHour = hourIndex;
    for (auto* cell : gridCells) {
        cell->setHourIndex(hourIndex);
    }
    if (sidebar) {
        sidebar->setHourIndex(hourIndex);
    }
}

void WeatherGridWidget::resetToAssam() {
    sharedLat = 26.2006;
    sharedLon = 92.5000;
    sharedZoom = 8;
    for (auto* cell : gridCells) {
        cell->setCenter(sharedLat, sharedLon);
        cell->setZoom(sharedZoom);
    }
    emit viewportChanged(sharedLat, sharedLon, sharedZoom);
}

void WeatherGridWidget::resetToIndia() {
    sharedLat = 22.0;
    sharedLon = 79.0;
    sharedZoom = 5;
    for (auto* cell : gridCells) {
        cell->setCenter(sharedLat, sharedLon);
        cell->setZoom(sharedZoom);
    }
    emit viewportChanged(sharedLat, sharedLon, sharedZoom);
}

void WeatherGridWidget::setViewport(double lat, double lon, int zoom) {
    sharedLat = std::clamp(lat, -85.0511, 85.0511);
    sharedLon = std::clamp(lon, -180.0, 180.0);
    sharedZoom = std::clamp(zoom, 2, 18);
    for (auto* cell : gridCells) {
        if (cell) {
            cell->blockSignals(true);
            cell->setCenter(sharedLat, sharedLon);
            cell->setZoom(sharedZoom);
            cell->blockSignals(false);
        }
    }
}

void WeatherGridWidget::setActive(bool active) {
    if (animTimer) {
        if (active) {
            if (!animTimer->isActive()) animTimer->start();
            update();
        } else {
            if (animTimer->isActive()) animTimer->stop();
        }
    }
}

void WeatherGridWidget::zoomIn() {
    sharedZoom = std::clamp(sharedZoom + 1, 2, 18);
    for (auto* cell : gridCells) {
        cell->setZoom(sharedZoom);
    }
    emit viewportChanged(sharedLat, sharedLon, sharedZoom);
}

void WeatherGridWidget::zoomOut() {
    sharedZoom = std::clamp(sharedZoom - 1, 2, 18);
    for (auto* cell : gridCells) {
        cell->setZoom(sharedZoom);
    }
    emit viewportChanged(sharedLat, sharedLon, sharedZoom);
}

void WeatherGridWidget::keyPressEvent(QKeyEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Plus || event->key() == Qt::Key_Equal || event->text() == "+") {
            zoomIn();
            event->accept();
            return;
        } else if (event->key() == Qt::Key_Minus || event->key() == Qt::Key_Underscore || event->text() == "-") {
            zoomOut();
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

void WeatherGridWidget::setSyncEnabled(bool sync) {
    syncViewports = sync;
}

void WeatherGridWidget::setTileProvider(MapCore::OnlineTileProvider provider) {
    sharedProvider = provider;
    for (auto* cell : gridCells) {
        cell->setTileProvider(provider);
    }
    if (sidebar) {
        sidebar->setTileProvider(provider);
    }
}

void WeatherGridWidget::setDarkMode(bool isDark) {
    setTileProvider(isDark ? MapCore::OnlineTileProvider::OpenStreetMap_Dark : MapCore::OnlineTileProvider::OpenStreetMap_Standard);
}

void WeatherGridWidget::onCellViewportChanged(double lat, double lon, int zoom, WeatherGridCellWidget* source) {
    if (!syncViewports) return;

    sharedLat = lat;
    sharedLon = lon;
    sharedZoom = zoom;

    for (auto* cell : gridCells) {
        if (cell != source) {
            cell->setCenter(lat, lon);
            cell->setZoom(zoom);
        }
    }
    emit viewportChanged(sharedLat, sharedLon, sharedZoom);
}

void WeatherGridWidget::onCellLocationClicked(double lat, double lon) {
    QString name = QString("%1° N, %2° E").arg(lat, 0, 'f', 4).arg(lon, 0, 'f', 4);
    fetchWeatherForLocation(lat, lon, name);
}

void WeatherGridWidget::onToggleMaximizeCell(WeatherGridCellWidget* cell) {
    if (maximizedCell == cell) {
        // Restore 6-Grid View
        maximizedCell = nullptr;
        for (int i = 0; i < 6; ++i) {
            gridCells[i]->setMaximized(false);
            gridCells[i]->show();
            int r = i / 3;
            int c = i % 3;
            gridLayout->addWidget(gridCells[i], r, c, 1, 1);
        }
    } else {
        // Maximize requested cell
        maximizedCell = cell;
        for (auto* c : gridCells) {
            if (c != cell) {
                c->hide();
            }
        }
        cell->setMaximized(true);
        gridLayout->addWidget(cell, 0, 0, 2, 3);
    }
}

void WeatherGridWidget::onForecastReceived(const MapCore::WeatherForecastData& data) {
    currentForecast = data;
    for (auto* cell : gridCells) {
        cell->setForecast(data, currentHour);
    }
    if (sidebar) {
        sidebar->setForecast(data);
    }
    if (weatherTimeline) {
        weatherTimeline->setWeatherForecast(data);
    }
    emit forecastUpdated(data);
}

} // namespace MapUI
