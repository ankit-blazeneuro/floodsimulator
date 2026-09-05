#include "SeaLevelTileWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <cmath>

namespace MapUI {

SeaLevelTileWidget::SeaLevelTileWidget(QWidget* parent)
    : OnlineTileWidget(parent) {
    setMouseTracking(true);
    setDarkMode(true); // Default to Dark tiles for maximum heatmap contrast

    // Continuous 30 FPS timer for surge animation and rising tide simulation
    animTimer = new QTimer(this);
    animTimer->setInterval(33);
    connect(animTimer, &QTimer::timeout, this, [this]() {
        animPhase = (animPhase + 1) % 10000;

        if (isSimulatingRise) {
            seaLevelRise += 0.06 * riseDirection;
            if (seaLevelRise >= 12.0) {
                riseDirection = -1.0;
            } else if (seaLevelRise <= 0.0) {
                seaLevelRise = 0.0;
                riseDirection = 1.0;
            }
            emit seaLevelRiseChanged(seaLevelRise);
        }

        update();
    });
    animTimer->start();
}

void SeaLevelTileWidget::setSeaLevelRise(double meters) {
    seaLevelRise = std::clamp(meters, 0.0, 25.0);
    emit seaLevelRiseChanged(seaLevelRise);
    update();
}

void SeaLevelTileWidget::setPalette(MapCore::HeatMapPalette pal) {
    currentPalette = pal;
    update();
}

void SeaLevelTileWidget::setHeatOpacity(float op) {
    heatOpacity = std::clamp(op, 0.05f, 1.0f);
    update();
}

void SeaLevelTileWidget::setShowContours(bool show) {
    showContours = show;
    update();
}

void SeaLevelTileWidget::setShowSurgeRipples(bool show) {
    showSurgeRipples = show;
    update();
}

void SeaLevelTileWidget::setShowHotspots(bool show) {
    showHotspots = show;
    update();
}

void SeaLevelTileWidget::setHeatmapEnabled(bool enabled) {
    heatmapEnabled = enabled;
    update();
}

void SeaLevelTileWidget::startRiseSimulation() {
    isSimulatingRise = true;
    riseDirection = 1.0;
    emit simulationStateChanged(true);
}

void SeaLevelTileWidget::pauseRiseSimulation() {
    isSimulatingRise = false;
    emit simulationStateChanged(false);
}

void SeaLevelTileWidget::resetRiseSimulation() {
    isSimulatingRise = false;
    seaLevelRise = 0.0;
    emit simulationStateChanged(false);
    emit seaLevelRiseChanged(0.0);
    update();
}

double SeaLevelTileWidget::calculateInundatedAreaKm2() const {
    if (seaLevelRise <= 0.0) return 0.0;

    int sampleN = 25;
    double countInundated = 0;
    double totalLand = 0;

    double latTop = screenToLat(0, 0);
    double latBottom = screenToLat(0, height());
    double lonLeft = screenToLon(0, 0);
    double lonRight = screenToLon(width(), 0);

    for (int y = 0; y < sampleN; ++y) {
        double lat = latBottom + y * (latTop - latBottom) / (sampleN - 1);
        for (int x = 0; x < sampleN; ++x) {
            double lon = lonLeft + x * (lonRight - lonLeft) / (sampleN - 1);
            double elev = MapCore::ElevationModel::getElevationMSL(lat, lon);
            if (elev >= 0.0) { // Was originally dry land
                totalLand += 1.0;
                if (elev <= seaLevelRise) {
                    countInundated += 1.0;
                }
            }
        }
    }

    double avgLat = (latTop + latBottom) * 0.5;
    double spanLatKm = std::abs(latTop - latBottom) * 111.0;
    double spanLonKm = std::abs(lonRight - lonLeft) * 111.0 * std::cos(avgLat * M_PI / 180.0);
    double totalAreaKm2 = spanLatKm * spanLonKm;

    if (totalLand <= 0.0) return 0.0;
    double landRatio = totalLand / (sampleN * sampleN);
    return (countInundated / totalLand) * (totalAreaKm2 * landRatio);
}

double SeaLevelTileWidget::calculateVulnerablePopulationMillions() const {
    double inundatedKm2 = calculateInundatedAreaKm2();
    // Indian coastal zones have high density (~800 to 1800 persons / km2)
    double pop = inundatedKm2 * 1250.0;
    return pop / 1000000.0;
}

void SeaLevelTileWidget::paintEvent(QPaintEvent* event) {
    // 1. Render Base Map Tiles
    OnlineTileWidget::paintEvent(event);

    if (!heatmapEnabled) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    int w = width();
    int h = height();
    if (w < 10 || h < 10) return;

    // 2. Generate Sea Level & Topography Heat Raster
    int gw = std::clamp(w / 4, 100, 260);
    int gh = std::clamp(h / 4, 70, 180);

    QImage heatImg(gw, gh, QImage::Format_ARGB32_Premultiplied);

    for (int y = 0; y < gh; ++y) {
        double sy = (gh > 1) ? y * static_cast<double>(h) / (gh - 1) : 0.0;
        double lat = screenToLat(w / 2.0, sy);
        QRgb* scan = reinterpret_cast<QRgb*>(heatImg.scanLine(y));

        for (int x = 0; x < gw; ++x) {
            double sx = (gw > 1) ? x * static_cast<double>(w) / (gw - 1) : 0.0;
            double lon = screenToLon(sx, sy);

            double elev = MapCore::ElevationModel::getElevationMSL(lat, lon);
            QColor col = MapCore::ElevationModel::getColorForElevation(elev, seaLevelRise, currentPalette, heatOpacity, animPhase);
            scan[x] = col.rgba();
        }
    }

    // Blend the smooth heat layer over the base map
    p.drawImage(rect(), heatImg);

    // 3. Render Inundation Frontline & Contours
    if (showContours) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);

        // Active Sea Level Floodline (Glowing Neon)
        if (seaLevelRise > 0.0) {
            QPen floodPen(QColor(0, 240, 255, 180), 2.0, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(floodPen);
            // Draw subtle indicator badge at the top
        }

        p.restore();
    }

    // 4. Render Animated Surge Ripple Arcs on Flooded Zones
    if (showSurgeRipples && seaLevelRise > 0.0) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);

        double ripplePhase = (animPhase % 60) / 60.0;
        double rRadius = 15.0 + ripplePhase * 40.0;
        int rAlpha = static_cast<int>((1.0 - ripplePhase) * 160.0);

        QPen ripplePen(QColor(56, 189, 248, rAlpha), 1.5, Qt::SolidLine);
        p.setPen(ripplePen);
        p.setBrush(Qt::NoBrush);

        const auto& hotspots = MapCore::ElevationModel::getCoastalPresets();
        for (const auto& spot : hotspots) {
            if (spot.baselineElevMSL <= seaLevelRise) {
                QPointF pt = geoToScreen(spot.lat, spot.lon);
                if (pt.x() >= -50 && pt.x() <= w + 50 && pt.y() >= -50 && pt.y() <= h + 50) {
                    p.drawEllipse(pt, rRadius, rRadius * 0.6);
                    p.drawEllipse(pt, rRadius * 0.5, rRadius * 0.3);
                }
            }
        }
        p.restore();
    }

    // 5. Render Coastal Hotspot Pins and Danger Badges
    if (showHotspots) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);

        const auto& hotspots = MapCore::ElevationModel::getCoastalPresets();
        for (const auto& spot : hotspots) {
            QPointF pt = geoToScreen(spot.lat, spot.lon);
            if (pt.x() >= 20 && pt.x() <= w - 20 && pt.y() >= 20 && pt.y() <= h - 20) {
                bool isSubmerged = (spot.baselineElevMSL <= seaLevelRise);
                double clearance = spot.baselineElevMSL - seaLevelRise;

                QColor badgeBg = isSubmerged ? QColor(225, 29, 72, 230)
                               : (clearance <= 3.0 ? QColor(217, 119, 6, 230)
                               : QColor(16, 185, 129, 210));

                // Outer Glowing Target Ring
                p.setPen(QPen(badgeBg, 1.8));
                p.setBrush(QColor(badgeBg.red(), badgeBg.green(), badgeBg.blue(), 60));
                p.drawEllipse(pt, 7.0, 7.0);

                // Inner Dot
                p.setBrush(QColor(255, 255, 255));
                p.setPen(Qt::NoPen);
                p.drawEllipse(pt, 2.5, 2.5);

                // Pill Tag
                if (getZoom() >= 7) {
                    QString statusText = isSubmerged ? QString("⚠️ SUBMERGED (-%1m)").arg(std::abs(clearance), 0, 'f', 1)
                                                     : QString("✓ %1m MSL").arg(spot.baselineElevMSL, 0, 'f', 1);

                    QFont tagFont("Segoe UI", 9, QFont::Bold);
                    p.setFont(tagFont);
                    QFontMetrics fm(tagFont);

                    int titleW = fm.horizontalAdvance(spot.name);
                    int statusW = fm.horizontalAdvance(statusText);
                    int cardW = std::max(titleW, statusW) + 16;
                    int cardH = 34;

                    QRect cardRect(static_cast<int>(pt.x()) - cardW / 2, static_cast<int>(pt.y()) - cardH - 12, cardW, cardH);

                    // Card Background
                    p.setPen(QPen(QColor(255, 255, 255, 40), 1));
                    p.setBrush(QColor(18, 18, 22, 225));
                    p.drawRoundedRect(cardRect, 6, 6);

                    // City Name
                    p.setPen(QColor(255, 255, 255));
                    p.drawText(cardRect.adjusted(8, 3, -8, -16), Qt::AlignLeft | Qt::AlignVCenter, spot.name);

                    // Status Line
                    p.setPen(isSubmerged ? QColor(251, 113, 133) : (clearance <= 3.0 ? QColor(252, 211, 77) : QColor(52, 211, 153)));
                    p.drawText(cardRect.adjusted(8, 16, -8, -3), Qt::AlignLeft | Qt::AlignVCenter, statusText);
                }
            }
        }
        p.restore();
    }

    // 6. Real-Time Crosshair & Tooltip at Live Cursor
    if (hasCursor && liveCursorPos.x() >= 0 && liveCursorPos.y() >= 0) {
        p.save();
        p.setRenderHint(QPainter::Antialiasing, true);

        int cx = liveCursorPos.x();
        int cy = liveCursorPos.y();

        // Crosshair Lines
        QPen crossPen(QColor(255, 255, 255, 120), 1.0, Qt::DashLine);
        p.setPen(crossPen);
        p.drawLine(cx - 14, cy, cx + 14, cy);
        p.drawLine(cx, cy - 14, cx, cy + 14);

        // Center Ring
        p.setPen(QPen(QColor(56, 189, 248, 220), 1.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPoint(cx, cy), 5, 5);

        // Compact Floating Hover Tooltip
        double curLat = screenToLat(cx, cy);
        double curLon = screenToLon(cx, cy);
        double curElev = MapCore::ElevationModel::getElevationMSL(curLat, curLon);
        double curClearance = curElev - seaLevelRise;
        bool isSub = (curElev <= seaLevelRise);

        QString t1 = QString("Elev: %1 m MSL").arg(curElev, 0, 'f', 1);
        QString t2 = isSub ? QString("⚠️ Inundated (-%1m)").arg(std::abs(curClearance), 0, 'f', 1)
                           : QString("✓ Clearance (+%1m)").arg(curClearance, 0, 'f', 1);

        QFont tipFont("Segoe UI", 9);
        p.setFont(tipFont);
        QFontMetrics tfm(tipFont);

        int tw = std::max(tfm.horizontalAdvance(t1), tfm.horizontalAdvance(t2)) + 16;
        int th = 38;

        int tx = cx + 16;
        int ty = cy + 16;
        if (tx + tw > w - 10) tx = cx - tw - 16;
        if (ty + th > h - 10) ty = cy - th - 16;

        QRect tipRect(tx, ty, tw, th);

        p.setPen(QPen(QColor(255, 255, 255, 30), 1));
        p.setBrush(QColor(15, 15, 18, 230));
        p.drawRoundedRect(tipRect, 6, 6);

        p.setPen(QColor(220, 220, 230));
        p.drawText(tipRect.adjusted(8, 3, -8, -18), Qt::AlignLeft | Qt::AlignVCenter, t1);

        p.setPen(isSub ? QColor(244, 63, 94) : QColor(52, 211, 153));
        p.drawText(tipRect.adjusted(8, 18, -8, -3), Qt::AlignLeft | Qt::AlignVCenter, t2);

        p.restore();
    }
}

void SeaLevelTileWidget::mouseMoveEvent(QMouseEvent* event) {
    OnlineTileWidget::mouseMoveEvent(event);

    liveCursorPos = event->pos();
    hasCursor = true;

    double lat = screenToLat(liveCursorPos.x(), liveCursorPos.y());
    double lon = screenToLon(liveCursorPos.x(), liveCursorPos.y());
    double elev = MapCore::ElevationModel::getElevationMSL(lat, lon);
    double clearance = elev - seaLevelRise;

    emit cursorElevationProbe(lat, lon, elev, clearance);
    update();
}

void SeaLevelTileWidget::leaveEvent(QEvent* event) {
    OnlineTileWidget::leaveEvent(event);
    hasCursor = false;
    update();
}

} // namespace MapUI
