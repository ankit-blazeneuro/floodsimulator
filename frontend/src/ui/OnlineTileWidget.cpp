#include "OnlineTileWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <cmath>
#include <algorithm>
#include <functional>
#include <iostream>

namespace MapUI {

OnlineTileWidget::OnlineTileWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    // Connect to central shared TileCacheManager
    connect(&MapCore::TileCacheManager::instance(), &MapCore::TileCacheManager::tileLoaded, this,
            [this](int provider, int zoom, int x, int y) {
                Q_UNUSED(provider);
                Q_UNUSED(zoom);
                Q_UNUSED(x);
                Q_UNUSED(y);
                update();
            });

    // Continuous 30 FPS fluid animation timer for river flow, ripple dynamics & danger dam beacons
    flowAnimTimer = new QTimer(this);
    flowAnimTimer->setInterval(33);
    connect(flowAnimTimer, &QTimer::timeout, this, [this]() {
        if (floodSimulation.isActive || showHelicopters || !liveHelicopters.empty() || !dangerDams.empty()) {
            flowAnimPhase = (flowAnimPhase + 1) % 10000;
            update();
        }
    });
    flowAnimTimer->start();

    // Initial center on India
    centerLat = 22.0;
    centerLon = 79.0;
    zoomLevel = 5;
}

void OnlineTileWidget::setCenter(double lat, double lon) {
    double newLat = std::clamp(lat, -85.0511, 85.0511);
    double newLon = std::clamp(lon, -180.0, 180.0);
    if (std::abs(centerLat - newLat) < 1e-7 && std::abs(centerLon - newLon) < 1e-7) return;
    centerLat = newLat;
    centerLon = newLon;
    emitViewportChanged();
    update();
}

void OnlineTileWidget::setZoom(int z) {
    int newZ = std::clamp(z, 2, 19);
    if (zoomLevel == newZ) return;
    zoomLevel = newZ;
    emitViewportChanged();
    update();
}

void OnlineTileWidget::setRenderingActive(bool active) {
    if (renderingActive == active) return;
    renderingActive = active;
    if (flowAnimTimer) {
        if (active) {
            if (!flowAnimTimer->isActive()) {
                flowAnimTimer->start();
            }
            update();
        } else {
            if (flowAnimTimer->isActive()) {
                flowAnimTimer->stop();
            }
        }
    }
}

void OnlineTileWidget::zoomIn() {
    setZoom(zoomLevel + 1);
}

void OnlineTileWidget::zoomOut() {
    setZoom(zoomLevel - 1);
}

void OnlineTileWidget::fitIndia() {
    setCenter(22.0, 79.0);
    setZoom(5);
}

void OnlineTileWidget::fitAssam() {
    setCenter(26.2006, 92.5000);
    setZoom(8);
}

void OnlineTileWidget::setTileProvider(OnlineTileProvider provider) {
    if (currentProvider != provider) {
        currentProvider = provider;
        update();
    }
}

void OnlineTileWidget::emitViewportChanged() {
    emit viewportChanged(centerLat, centerLon, zoomLevel);
}

// --- Coordinate conversions (Slippy Map Tilenames) ---

double OnlineTileWidget::lonToTileX(double lon, int zoom) {
    return (lon + 180.0) / 360.0 * std::pow(2.0, zoom);
}

double OnlineTileWidget::latToTileY(double lat, int zoom) {
    double latRad = lat * M_PI / 180.0;
    return (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * std::pow(2.0, zoom);
}

double OnlineTileWidget::tileXToLon(double x, int zoom) {
    return x / std::pow(2.0, zoom) * 360.0 - 180.0;
}

double OnlineTileWidget::tileYToLat(double y, int zoom) {
    double n = M_PI - 2.0 * M_PI * y / std::pow(2.0, zoom);
    return 180.0 / M_PI * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
}

// --- Tile fetching & caching (Shared Single OSM Fetch Engine) ---

QPixmap* OnlineTileWidget::getTile(int zoom, int x, int y) {
    return MapCore::TileCacheManager::instance().getTile(
        static_cast<MapCore::OnlineTileProvider>(currentProvider), zoom, x, y);
}

// --- Paint ---

void OnlineTileWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::Antialiasing, true);

    int w = width();
    int h = height();
    double cx = w / 2.0;
    double cy = h / 2.0;

    // Background adapting to dark / light theme
    painter.fillRect(rect(), isDarkMode() ? QColor(24, 24, 27) : QColor(240, 240, 245));

    // Calculate center tile position
    double centerTileX = lonToTileX(centerLon, zoomLevel);
    double centerTileY = latToTileY(centerLat, zoomLevel);

    // Pixel offset from center of widget to top-left corner of the center tile
    double offsetX = cx - (centerTileX - std::floor(centerTileX)) * TILE_SIZE;
    double offsetY = cy - (centerTileY - std::floor(centerTileY)) * TILE_SIZE;

    int centerTileXInt = static_cast<int>(std::floor(centerTileX));
    int centerTileYInt = static_cast<int>(std::floor(centerTileY));

    // Cover maximum diagonal distance to support full 360° rotation without black borders
    double maxDiag = std::sqrt(w * w + h * h) / 2.0;
    int tilesRadius = static_cast<int>(std::ceil(maxDiag / TILE_SIZE)) + 1;

    // 1. Begin Rotated Map Scene (Centered on Screen Middle Crosshair)
    painter.save();
    painter.translate(cx, cy);
    painter.rotate(rotationAngle);
    painter.translate(-cx, -cy);

    // Draw Map Tiles
    for (int dy = -tilesRadius; dy <= tilesRadius; ++dy) {
        for (int dx = -tilesRadius; dx <= tilesRadius; ++dx) {
            int tileX = centerTileXInt + dx;
            int tileY = centerTileYInt + dy;

            int drawX = static_cast<int>(offsetX + dx * TILE_SIZE);
            int drawY = static_cast<int>(offsetY + dy * TILE_SIZE);

            QPixmap* tile = getTile(zoomLevel, tileX, tileY);
            if (tile && !tile->isNull()) {
                painter.drawPixmap(drawX, drawY, TILE_SIZE, TILE_SIZE, *tile);
            } else {
                QRect tileRect(drawX, drawY, TILE_SIZE, TILE_SIZE);
                painter.fillRect(tileRect, isDarkMode() ? QColor(32, 33, 36) : QColor(230, 230, 235));
                painter.setPen(QPen(isDarkMode() ? QColor(45, 48, 52) : QColor(210, 210, 215), 1));
                painter.drawRect(tileRect);
            }
        }
    }

    // Container for Highest Z-Index Tooltips & Floating Badges (Rendered over all geometry)
    std::vector<std::function<void(QPainter&)>> topTooltips;

    // --- Render Indian Dams (#54D59A Mint Green Dots with 3D-Style Viewport Frustum Culling) ---
    if (damManager && showDams && damManager->hasData()) {
        const QColor dotColor(0x54, 0xD5, 0x9A); // #54D59A
        const QColor glowColor(0x54, 0xD5, 0x9A, 90);
        const QColor dotBorder(0x1B, 0x5E, 0x3E, 200);

        // Calculate Viewport Bounding Box covering full rotation diagonal with buffer
        const double bufferPx = maxDiag + 48.0;
        double leftTileX = centerTileX - bufferPx / TILE_SIZE;
        double rightTileX = centerTileX + bufferPx / TILE_SIZE;
        double topTileY = centerTileY - bufferPx / TILE_SIZE;
        double bottomTileY = centerTileY + bufferPx / TILE_SIZE;

        double minLon = tileXToLon(leftTileX, zoomLevel);
        double maxLon = tileXToLon(rightTileX, zoomLevel);
        double maxLat = tileYToLat(topTileY, zoomLevel);
        double minLat = tileYToLat(bottomTileY, zoomLevel);

        std::vector<const MapCore::DamPoint*> visibleDams;
        damManager->getDamsInBbox(minLat, minLon, maxLat, maxLon, visibleDams);

        double radius = (zoomLevel <= 6) ? 3.0 : std::min(5.5, 3.0 + (zoomLevel - 6) * 0.35);

        for (const auto* dam : visibleDams) {
            double tileX = lonToTileX(dam->lon, zoomLevel);
            double tileY = latToTileY(dam->lat, zoomLevel);

            double sx = cx + (tileX - centerTileX) * TILE_SIZE;
            double sy = cy + (tileY - centerTileY) * TILE_SIZE;

            painter.setPen(Qt::NoPen);
            painter.setBrush(glowColor);
            painter.drawEllipse(QPointF(sx, sy), radius + 1.2, radius + 1.2);

            painter.setPen(QPen(dotBorder, 0.75));
            painter.setBrush(dotColor);
            painter.drawEllipse(QPointF(sx, sy), radius, radius);

            // Defer Dam Name Tooltip to Top Z-Index Layer
            if (zoomLevel >= 8 && !dam->name.isEmpty()) {
                topTooltips.push_back([sx, sy, radius, zoom = zoomLevel, dName = dam->name](QPainter& p) {
                    QFont damFont("Segoe UI", (zoom >= 11) ? 8 : 7, QFont::DemiBold);
                    p.setFont(damFont);
                    QFontMetricsF dfm(damFont);
                    double tw = dfm.horizontalAdvance(dName);
                    double th = dfm.height();

                    QRectF textBgRect(sx + radius + 4, sy - th / 2.0 - 1, tw + 8, th + 3);
                    p.setBrush(QColor(24, 24, 27, 255)); // 100% opacity solid gray
                    p.setPen(QPen(QColor(63, 63, 70, 255), 1.0)); // shadcn zinc-700 border
                    p.drawRoundedRect(textBgRect, 5.0, 5.0);

                    p.setPen(QColor(244, 244, 245)); // shadcn zinc-100
                    p.drawText(textBgRect, Qt::AlignCenter, dName);
                });
            }
        }
    }

    // --- Render Pulsing Red Dot Animations for All Dams Under Danger ---
    if (!dangerDams.empty()) {
        for (const auto& marker : dangerDams) {
            double tileX = lonToTileX(marker.dam.lon, zoomLevel);
            double tileY = latToTileY(marker.dam.lat, zoomLevel);
            double sx = cx + (tileX - centerTileX) * TILE_SIZE;
            double sy = cy + (tileY - centerTileY) * TILE_SIZE;
            QPointF sPos(sx, sy);

            double pulse = std::fmod((flowAnimPhase * 0.9), 22.0);
            double pulseAlpha = std::max(0, 220 - static_cast<int>(pulse * 9.5));

            QColor pColor = QColor(marker.alertColor);
            QColor pGlow = pColor;
            pGlow.setAlpha(static_cast<int>(pulseAlpha * 0.35));

            // Expanding outer animated radar aura ring
            painter.setPen(QPen(QColor(pColor.red(), pColor.green(), pColor.blue(), static_cast<int>(pulseAlpha)), 2.5));
            painter.setBrush(pGlow);
            painter.drawEllipse(sPos, 7.0 + pulse, 7.0 + pulse);

            // High-visibility inner glowing red hazard beacon
            painter.setPen(QPen(Qt::white, 2.0));
            painter.setBrush(pColor);
            painter.drawEllipse(sPos, 6.5, 6.5);

            topTooltips.push_back([sx, sy, dName = marker.dam.name, probPct = int(marker.failureProbability * 100.0), alert = marker.alertLevel, aColor = marker.alertColor](QPainter& p) {
                QFont dFont("Segoe UI", 8, QFont::Bold);
                p.setFont(dFont);
                QFontMetricsF dfm(dFont);
                QString label = QString("⚠️ %1 · P(Breach)=%2% (%3)").arg(dName).arg(probPct).arg(alert);
                double tw = dfm.horizontalAdvance(label);
                double th = dfm.height();

                QRectF textBgRect(sx - tw / 2.0 - 6, sy - 16 - th, tw + 12, th + 4);
                p.setBrush(QColor(24, 24, 27, 255));
                p.setPen(QPen(QColor(aColor), 1.2));
                p.drawRoundedRect(textBgRect, 5.0, 5.0);

                p.setPen(QColor(aColor));
                p.drawText(textBgRect, Qt::AlignCenter, label);
            });
        }
    }

    // --- Render Selected Dams Highlight (Glow Target Rings & Title Labels Rotated with Map) ---
    if (!selectedDams.empty()) {
        for (const auto* dam : selectedDams) {
            if (!dam) continue;
            double tileX = lonToTileX(dam->lon, zoomLevel);
            double tileY = latToTileY(dam->lat, zoomLevel);
            double sx = cx + (tileX - centerTileX) * TILE_SIZE;
            double sy = cy + (tileY - centerTileY) * TILE_SIZE;
            QPointF sPos(sx, sy);

            painter.setPen(QPen(QColor(253, 214, 99, 220), 1.8));
            painter.setBrush(QColor(253, 214, 99, 50));
            painter.drawEllipse(sPos, 9.0, 9.0);

            painter.setPen(QPen(Qt::white, 1.5));
            painter.setBrush(QColor(253, 214, 99));
            painter.drawEllipse(sPos, 3.5, 3.5);

            if (!dam->name.isEmpty()) {
                topTooltips.push_back([sx, sy, sName = dam->name](QPainter& p) {
                    QFont selFont("Segoe UI", 9, QFont::DemiBold);
                    p.setFont(selFont);
                    QFontMetricsF sfm(selFont);
                    double tw = sfm.horizontalAdvance(sName);
                    double th = sfm.height();

                    QRectF selBgRect(sx + 10, sy - th / 2.0 - 2, tw + 10, th + 4);
                    p.setBrush(QColor(24, 24, 27, 255)); // 100% opacity solid gray
                    p.setPen(QPen(QColor(63, 63, 70, 255), 1.0)); // shadcn zinc-700 border
                    p.drawRoundedRect(selBgRect, 6.0, 6.0);

                    p.setPen(QColor(244, 244, 245));
                    p.drawText(selBgRect, Qt::AlignCenter, sName);
                });
            }
        }
    }

    // --- Render Dotted Border Selection Box (Select Tool) ---
    if (isBoxSelecting) {
        QPointF unrotStart = unrotatePoint(boxSelectStart);
        QPointF unrotCurrent = unrotatePoint(boxSelectCurrent);
        QRectF boxRect = QRectF(unrotStart, unrotCurrent).normalized();

        painter.setBrush(QColor(138, 180, 248, 38));

        QPen dottedPen(QColor(138, 180, 248), 1.5, Qt::CustomDashLine);
        QList<qreal> dashes;
        dashes << 4.0 << 4.0;
        dottedPen.setDashPattern(dashes);
        painter.setPen(dottedPen);
        painter.drawRect(boxRect);

        double minLon = screenToLon(boxSelectStart.x(), boxSelectStart.y());
        double maxLon = screenToLon(boxSelectCurrent.x(), boxSelectCurrent.y());
        double minLat = screenToLat(boxSelectStart.x(), boxSelectStart.y());
        double maxLat = screenToLat(boxSelectCurrent.x(), boxSelectCurrent.y());
        if (minLon > maxLon) std::swap(minLon, maxLon);
        if (minLat > maxLat) std::swap(minLat, maxLat);

        std::vector<const MapCore::DamPoint*> previewDams;
        if (damManager && showDams && damManager->hasData()) {
            damManager->getDamsInBbox(minLat, minLon, maxLat, maxLon, previewDams);
        }

        QString badgeText = (previewDams.empty())
            ? QString("%1 × %2 px").arg(qRound(boxRect.width())).arg(qRound(boxRect.height()))
            : QString("%1 dams (%2 × %3 px)").arg(previewDams.size()).arg(qRound(boxRect.width())).arg(qRound(boxRect.height()));

        topTooltips.push_back([boxRect, badgeText](QPainter& p) {
            QFont badgeFont("Segoe UI", 8, QFont::Bold);
            p.setFont(badgeFont);
            QFontMetricsF bfm(badgeFont);
            QRectF badgeRect(boxRect.right() - bfm.horizontalAdvance(badgeText) - 10, boxRect.bottom() + 4,
                             bfm.horizontalAdvance(badgeText) + 10, bfm.height() + 4);

            p.setBrush(QColor(20, 20, 24, 230));
            p.setPen(QPen(QColor(138, 180, 248, 180), 1.0));
            p.drawRoundedRect(badgeRect, 3.0, 3.0);

            p.setPen(QColor(240, 240, 245));
            p.drawText(badgeRect, Qt::AlignCenter, badgeText);
        });
    }

    // --- Render Distance Measurement Path & Pins (Ruler Tool) ---
    if (measureMode && (!measurePoints.empty() || hasLiveMouse)) {
        const QColor rulerLineColor(138, 180, 248); // Neon Blue
        const QColor rulerPointColor(255, 255, 255);

        // 1. When >= 3 points, draw enclosed area polygon (15% opacity) & compute geodesic area
        if (measurePoints.size() >= 3) {
            QPolygonF rulerPoly;
            for (const auto& pt : measurePoints) {
                rulerPoly.append(geoToScreen(pt.x(), pt.y()));
            }

            // 15% opacity fill (38/255 = 14.9%)
            painter.setBrush(QColor(138, 180, 248, 38));
            painter.setPen(QPen(QColor(138, 180, 248, 140), 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPolygon(rulerPoly);

            // Compute exact geodesic polygon area on Earth surface (local tangent projection)
            double meanLat = 0.0;
            for (const auto& p : measurePoints) meanLat += p.x();
            meanLat /= static_cast<double>(measurePoints.size());
            double meanLatRad = meanLat * M_PI / 180.0;

            const double R = 6378137.0; // Earth radius in meters
            double cosLat = std::cos(meanLatRad);

            std::vector<QPointF> localMeters;
            localMeters.reserve(measurePoints.size());
            for (const auto& p : measurePoints) {
                double x = (p.y() * M_PI / 180.0) * R * cosLat;
                double y = (p.x() * M_PI / 180.0) * R;
                localMeters.push_back(QPointF(x, y));
            }

            double areaM2 = 0.0;
            size_t n = localMeters.size();
            for (size_t i = 0; i < n; ++i) {
                size_t j = (i + 1) % n;
                areaM2 += localMeters[i].x() * localMeters[j].y() - localMeters[j].x() * localMeters[i].y();
            }
            areaM2 = std::abs(areaM2) * 0.5;

            // Formatted area text
            QString areaText;
            if (areaM2 >= 1000000.0) {
                double areaKm2 = areaM2 / 1000000.0;
                double areaHa = areaM2 / 10000.0;
                areaText = QString("Area: %1 km² (%2 ha)").arg(areaKm2, 0, 'f', 2).arg(areaHa, 0, 'f', 1);
            } else if (areaM2 >= 10000.0) {
                double areaHa = areaM2 / 10000.0;
                areaText = QString("Area: %1 ha (%2 m²)").arg(areaHa, 0, 'f', 2).arg(QLocale(QLocale::English).toString(qRound(areaM2)));
            } else {
                areaText = QString("Area: %1 m²").arg(QLocale(QLocale::English).toString(qRound(areaM2)));
            }

            // Polygon Centroid in screen coordinates
            QPointF centerScreen(0, 0);
            for (const auto& pt : rulerPoly) {
                centerScreen += pt;
            }
            centerScreen /= static_cast<double>(rulerPoly.size());

            topTooltips.push_back([centerScreen, areaText, rulerLineColor](QPainter& p) {
                QFont aFont("Segoe UI", 9, QFont::Bold);
                p.setFont(aFont);
                QFontMetricsF afm(aFont);
                double bw = afm.horizontalAdvance(areaText) + 16.0;
                double bh = afm.height() + 8.0;
                QRectF aRect(centerScreen.x() - bw / 2.0, centerScreen.y() - bh / 2.0, bw, bh);

                p.setBrush(QColor(24, 24, 27, 255)); // 100% solid dark zinc card background
                p.setPen(QPen(rulerLineColor, 1.4)); // Neon blue border
                p.drawRoundedRect(aRect, 5.0, 5.0);

                p.setPen(QColor(244, 244, 245)); // Clean white text
                p.drawText(aRect, Qt::AlignCenter, areaText);
            });
        }

        // 2. Draw solid lines connecting all placed pins
        if (measurePoints.size() >= 2) {
            QPainterPath path;
            path.moveTo(geoToScreen(measurePoints[0].x(), measurePoints[0].y()));
            for (size_t i = 1; i < measurePoints.size(); ++i) {
                path.lineTo(geoToScreen(measurePoints[i].x(), measurePoints[i].y()));
            }

            painter.setPen(QPen(QColor(0, 0, 0, 180), 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);

            painter.setPen(QPen(rulerLineColor, 2.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }

        // 2. Draw live dashed elastic line to current cursor
        if (!measurePoints.empty() && hasLiveMouse && !isDragging) {
            QPointF lastScreenPt = geoToScreen(measurePoints.back().x(), measurePoints.back().y());
            QPointF cursorPt = unrotatePoint(liveMousePos);

            painter.setPen(QPen(QColor(255, 255, 255, 200), 1.8, Qt::DashLine, Qt::RoundCap));
            painter.drawLine(lastScreenPt, cursorPt);

            double curLat = screenToLat(liveMousePos.x(), liveMousePos.y());
            double curLon = screenToLon(liveMousePos.x(), liveMousePos.y());
            double segDistM = haversineDistanceM(measurePoints.back().x(), measurePoints.back().y(), curLat, curLon);

            QString liveText = (segDistM < 1000.0)
                ? QString("+%1 m").arg(qRound(segDistM))
                : QString("+%1 km").arg(segDistM / 1000.0, 0, 'f', 2);

            topTooltips.push_back([cursorPt, liveText, rulerLineColor](QPainter& p) {
                QFont liveFont("Segoe UI", 8, QFont::Bold);
                p.setFont(liveFont);
                QFontMetricsF lfm(liveFont);
                QRectF liveRect(cursorPt.x() + 12, cursorPt.y() - 10, lfm.horizontalAdvance(liveText) + 12, lfm.height() + 4);

                p.setBrush(QColor(24, 24, 27, 255));
                p.setPen(QPen(rulerLineColor, 1.0));
                p.drawRoundedRect(liveRect, 4, 4);

                p.setPen(QColor(220, 220, 225));
                p.drawText(liveRect, Qt::AlignCenter, liveText);
            });
        }

        // 3. Draw Waypoint Pins & Cumulative Distance Badges
        double totalDistM = 0.0;
        for (size_t i = 0; i < measurePoints.size(); ++i) {
            QPointF ptScreen = geoToScreen(measurePoints[i].x(), measurePoints[i].y());

            if (i > 0) {
                totalDistM += haversineDistanceM(measurePoints[i-1].x(), measurePoints[i-1].y(),
                                                 measurePoints[i].x(), measurePoints[i].y());
            }

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 140));
            painter.drawEllipse(ptScreen + QPointF(0, 2), 6.0, 6.0);

            painter.setBrush(rulerLineColor);
            painter.setPen(QPen(rulerPointColor, 2.5));
            painter.drawEllipse(ptScreen, 5.5, 5.5);

            QString distText = (i == 0) ? "Start" : ((totalDistM < 1000.0)
                ? QString("%1 m").arg(qRound(totalDistM))
                : QString("%1 km").arg(totalDistM / 1000.0, 0, 'f', 2));

            topTooltips.push_back([ptScreen, distText](QPainter& p) {
                QFont badgeFont("Segoe UI", 9, QFont::Bold);
                p.setFont(badgeFont);
                QFontMetricsF bfm(badgeFont);
                QRectF badgeRect(ptScreen.x() + 10, ptScreen.y() - 12, bfm.horizontalAdvance(distText) + 14, bfm.height() + 5);

                p.setBrush(QColor(24, 24, 27, 255));
                p.setPen(QPen(QColor(255, 255, 255, 180), 1.0));
                p.drawRoundedRect(badgeRect, 4, 4);

                p.setPen(Qt::white);
                p.drawText(badgeRect, Qt::AlignCenter, distText);
            });
        }
    }

    // --- Render Hydrodynamic River Flow, Trapped Water Pools & Saddle Overtopping Cascades ---
    auto renderSingleFloodSimulation = [&](const MapCore::FloodSimulationState& sim, bool showDetailTooltips) {
        if (!sim.isActive) return;
        const auto* slice = MapCore::DamFloodSimulator::getTimeSlice(sim, sim.currentMinute);
        if (!slice) return;

        auto toCanvasPoint = [&](double lat, double lon) -> QPointF {
            double tX = lonToTileX(lon, zoomLevel);
            double tY = latToTileY(lat, zoomLevel);
            return QPointF(cx + (tX - centerTileX) * TILE_SIZE, cy + (tY - centerTileY) * TILE_SIZE);
        };

        // 1. Dam Breach Origin Beacon (Breach Starting Point)
        if (!sim.rawNodes.empty()) {
            QPointF breachScreen = toCanvasPoint(sim.rawNodes[0].lat, sim.rawNodes[0].lon);
            double breachPulse = std::fmod((flowAnimPhase * 0.7), 18.0);

            // Pulsing red-orange breach warning beacon
            painter.setPen(QPen(QColor(239, 68, 68, std::max(0, 220 - static_cast<int>(breachPulse * 10))), 1.8));
            painter.setBrush(QColor(239, 68, 68, std::max(0, 50 - static_cast<int>(breachPulse * 2))));
            painter.drawEllipse(breachScreen, 7.0 + breachPulse, 7.0 + breachPulse);

            painter.setPen(QPen(QColor(255, 255, 255), 1.5));
            painter.setBrush(QColor(239, 68, 68));
            painter.drawEllipse(breachScreen, 4.0, 4.0);

            if (sim.currentMinute == 0 && zoomLevel >= 8 && showDetailTooltips) {
                QString breachLabel = QString("%1 · Breach Origin · T+0m").arg(sim.dam.name);
                topTooltips.push_back([breachScreen, breachLabel](QPainter& p) {
                    QFont bFont("Segoe UI", 7, QFont::DemiBold);
                    p.setFont(bFont);
                    QFontMetricsF bfm(bFont);
                    QRectF bRect(breachScreen.x() - bfm.horizontalAdvance(breachLabel) / 2.0 - 6,
                                 breachScreen.y() - bfm.height() - 10,
                                 bfm.horizontalAdvance(breachLabel) + 12, bfm.height() + 4);
                    p.setBrush(QColor(24, 24, 27, 255));
                    p.setPen(QPen(QColor(239, 68, 68, 200), 1.0));
                    p.drawRoundedRect(bRect, 4.0, 4.0);
                    p.setPen(QColor(244, 244, 245));
                    p.drawText(bRect, Qt::AlignCenter, breachLabel);
                });
            }
        }

        // 2. Trapped Water Pools in Topographic Basins (Depression Storage)
        for (const auto& pool : slice->trappedPools) {
            if (pool.poolPolygon.size() >= 3) {
                QPolygonF cPoly;
                cPoly.reserve(pool.poolPolygon.size());
                for (const auto& pt : pool.poolPolygon) {
                    cPoly.append(toCanvasPoint(pt.x(), pt.y()));
                }

                // Outer fringe (turquoise water boundary)
                painter.setPen(QPen(QColor(0, 229, 255, 180), 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.setBrush(QColor(0, 188, 212, 115)); // Turquoise Water
                painter.drawPolygon(cPoly);

                // Mid-depth inner water body
                QPointF poolCenterScreen = toCanvasPoint(pool.centerPos.x(), pool.centerPos.y());
                QTransform t;
                t.translate(poolCenterScreen.x(), poolCenterScreen.y());
                t.scale(0.65, 0.65);
                t.translate(-poolCenterScreen.x(), -poolCenterScreen.y());
                QPolygonF midPoly = t.map(cPoly);
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(24, 118, 209, 150)); // Royal blue
                painter.drawPolygon(midPoly);

                // Deep core
                t.reset();
                t.translate(poolCenterScreen.x(), poolCenterScreen.y());
                t.scale(0.35, 0.35);
                t.translate(-poolCenterScreen.x(), -poolCenterScreen.y());
                QPolygonF deepPoly = t.map(cPoly);
                painter.setBrush(QColor(10, 50, 140, 190)); // Deep Indigo
                painter.drawPolygon(deepPoly);

                // Animated expanding fluid ripple inside trapped pool
                double ripplePhase = std::fmod((flowAnimPhase * 0.8), 24.0);
                painter.setPen(QPen(QColor(255, 255, 255, std::max(0, 180 - static_cast<int>(ripplePhase * 7))), 1.0));
                painter.setBrush(Qt::NoBrush);
                painter.drawEllipse(poolCenterScreen, 6.0 + ripplePhase, 4.0 + ripplePhase * 0.7);

                // Defer Trapped Basin Tooltip to Top Z-Index Layer
                if (zoomLevel >= 8 && showDetailTooltips) {
                    QString poolLabel = QString("%1 · %2 MCM · %3m")
                        .arg(pool.name.section(':', 0, 0))
                        .arg(pool.volumeMCM, 0, 'f', 1)
                        .arg(pool.depthM, 0, 'f', 1);

                    topTooltips.push_back([poolCenterScreen, poolLabel](QPainter& p) {
                        QFont pFont("Segoe UI", 7, QFont::DemiBold);
                        p.setFont(pFont);
                        QFontMetricsF pfm(pFont);
                        QRectF pRect(poolCenterScreen.x() - pfm.horizontalAdvance(poolLabel) / 2.0 - 6,
                                     poolCenterScreen.y() - pfm.height() / 2.0 - 2,
                                     pfm.horizontalAdvance(poolLabel) + 12, pfm.height() + 4);
                        p.setBrush(QColor(24, 24, 27, 255)); // 100% opacity solid gray
                        p.setPen(QPen(QColor(63, 63, 70, 255), 1.0));
                        p.drawRoundedRect(pRect, 5.0, 5.0);
                        p.setPen(QColor(244, 244, 245)); // shadcn zinc-100 text
                        p.drawText(pRect, Qt::AlignCenter, poolLabel);
                    });
                }

                // 3. Saddle Overtopping Cascade (Water spills over ridge into next reach)
                if (pool.isOvertopping) {
                    QPointF spillScreen = toCanvasPoint(pool.saddleSpillPos.x(), pool.saddleSpillPos.y());
                    double spillPulse = std::fmod((flowAnimPhase * 1.2), 16.0);
                    painter.setPen(QPen(QColor(0, 229, 255, 240), 2.2, Qt::SolidLine, Qt::RoundCap));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawEllipse(spillScreen, 6.0 + spillPulse, 6.0 + spillPulse);

                    painter.setPen(QPen(QColor(253, 214, 99, 230), 1.5));
                    painter.setBrush(QColor(253, 214, 99));
                    painter.drawEllipse(spillScreen, 3.0, 3.0);

                    if (zoomLevel >= 8 && showDetailTooltips) {
                        QString spillText = "Saddle Spill";
                        topTooltips.push_back([spillScreen, spillText](QPainter& p) {
                            QFont sFont("Segoe UI", 7, QFont::DemiBold);
                            p.setFont(sFont);
                            QFontMetricsF sfm(sFont);
                            QRectF sRect(spillScreen.x() + 8, spillScreen.y() - sfm.height() / 2.0 - 2,
                                         sfm.horizontalAdvance(spillText) + 10, sfm.height() + 4);
                            p.setBrush(QColor(24, 24, 27, 255)); // 100% opacity solid gray
                            p.setPen(QPen(QColor(63, 63, 70, 255), 1.0));
                            p.drawRoundedRect(sRect, 5.0, 5.0);
                            p.setPen(QColor(253, 214, 99));
                            p.drawText(sRect, Qt::AlignCenter, spillText);
                        });
                    }
                }
            }
        }

        // 4. Area under Active Flood Displacement Line & Streamline Flow (10% Opacity)
        if (slice->riverStreamline.size() >= 2) {
            std::vector<QPointF> activePts;
            activePts.reserve(slice->riverStreamline.size());
            for (size_t i = 0; i < slice->riverStreamline.size(); ++i) {
                activePts.push_back(toCanvasPoint(slice->riverStreamline[i].x(), slice->riverStreamline[i].y()));
            }

            QPolygonF displacementAreaPoly;
            float baseDisplacementW = std::clamp(22.0f + static_cast<float>(slice->maxDepthM) * 1.8f, 16.0f, 64.0f);

            for (size_t i = 0; i < activePts.size(); ++i) {
                QPointF dir;
                if (i == 0) dir = activePts[1] - activePts[0];
                else if (i + 1 == activePts.size()) dir = activePts[i] - activePts[i - 1];
                else dir = activePts[i + 1] - activePts[i - 1];

                float len = std::hypot(dir.x(), dir.y());
                if (len > 0.001f) {
                    float taper = 1.0f - 0.35f * (static_cast<float>(i) / static_cast<float>(activePts.size()));
                    float w = baseDisplacementW * taper;
                    QPointF norm(-dir.y() / len, dir.x() / len);
                    displacementAreaPoly.append(activePts[i] + norm * (w / 2.0f));
                }
            }

            // Leading front displacement cap
            if (!activePts.empty()) {
                displacementAreaPoly.append(activePts.back());
            }

            for (int i = static_cast<int>(activePts.size()) - 1; i >= 0; --i) {
                QPointF dir;
                if (i == 0) dir = activePts[1] - activePts[0];
                else if (i + 1 == static_cast<int>(activePts.size())) dir = activePts[i] - activePts[i - 1];
                else dir = activePts[i + 1] - activePts[i - 1];

                float len = std::hypot(dir.x(), dir.y());
                if (len > 0.001f) {
                    float taper = 1.0f - 0.35f * (static_cast<float>(i) / static_cast<float>(activePts.size()));
                    float w = baseDisplacementW * taper;
                    QPointF norm(-dir.y() / len, dir.x() / len);
                    displacementAreaPoly.append(activePts[i] - norm * (w / 2.0f));
                }
            }

            if (displacementAreaPoly.size() >= 3) {
                painter.setBrush(QColor(0, 210, 255, 26)); // 10% opacity area under displacement line
                painter.setPen(QPen(QColor(0, 229, 255, 75), 1.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawPolygon(displacementAreaPoly);
            }

            // Animated Flowing River Streamline & Moving Particles
            QPainterPath activeReachPath;
            activeReachPath.moveTo(activePts[0]);
            for (size_t i = 1; i < activePts.size(); ++i) {
                activeReachPath.lineTo(activePts[i]);
            }

            painter.setPen(QPen(QColor(0, 188, 212, 220), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(activeReachPath);

            // Flowing River Streamline Dashes
            QPen flowPen(QColor(255, 255, 255, 220), 1.8, Qt::CustomDashLine, Qt::RoundCap);
            QList<qreal> dashes;
            dashes << 6.0 << 6.0;
            flowPen.setDashPattern(dashes);
            flowPen.setDashOffset(flowAnimPhase * 1.5);
            painter.setPen(flowPen);
            painter.drawPath(activeReachPath);

            // Moving Particles
            int totalPts = static_cast<int>(activePts.size());
            for (int i = 0; i < totalPts; ++i) {
                int animIdx = (i + (flowAnimPhase / 2)) % totalPts;
                if (animIdx % 3 == 0) {
                    QPointF pPt = activePts[animIdx];
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(QColor(255, 255, 255, 220));
                    painter.drawEllipse(pPt, 2.2, 2.2);
                    painter.setBrush(QColor(0, 229, 255, 90));
                    painter.drawEllipse(pPt, 4.5, 4.5);
                }
            }

            if (showDetailTooltips) {
                for (size_t i = 0; i < activePts.size(); ++i) {
                    if (i > 0 && i % 8 == 0 && i < sim.rawNodes.size()) {
                        QPointF cPt = activePts[i];
                        double dist = sim.rawNodes[i].distanceKm;
                        painter.setPen(QPen(QColor(253, 214, 99, 220), 1.0));
                        painter.setBrush(QColor(253, 214, 99, 230));
                        painter.drawEllipse(cPt, 2.8, 2.8);

                        QString dLabel = QString("%1 km").arg(qRound(dist));
                        topTooltips.push_back([cPt, dLabel](QPainter& p) {
                            QFont mFont("Segoe UI", 7, QFont::DemiBold);
                            p.setFont(mFont);
                            QFontMetricsF mfm(mFont);
                            QRectF mRect(cPt.x() + 6, cPt.y() - mfm.height() / 2.0 - 1, mfm.horizontalAdvance(dLabel) + 8, mfm.height() + 2);
                            p.setBrush(QColor(24, 24, 27, 255));
                            p.setPen(QPen(QColor(63, 63, 70, 255), 1.0));
                            p.drawRoundedRect(mRect, 4.0, 4.0);
                            p.setPen(QColor(228, 228, 231));
                            p.drawText(mRect, Qt::AlignCenter, dLabel);
                        });
                    }
                }
            }
        }

        // 5. Leading Wave Front Distance Marker & Tooltip
        if (slice->frontDistanceKm > 0.05) {
            QPointF frontScreen = toCanvasPoint(slice->leadingFrontPos.x(), slice->leadingFrontPos.y());
            double frontPulse = std::fmod((flowAnimPhase * 0.6), 14.0);
            painter.setPen(QPen(QColor(0, 229, 255, 240), 2.0));
            painter.setBrush(QColor(0, 229, 255, 40));
            painter.drawEllipse(frontScreen, 8.0 + frontPulse, 8.0 + frontPulse);

            painter.setPen(QPen(QColor(253, 214, 99), 1.5));
            painter.setBrush(QColor(253, 214, 99));
            painter.drawEllipse(frontScreen, 3.5, 3.5);

            if (showDetailTooltips) {
                QString frontBadge = QString("%1 · %2 km · T + %3m")
                    .arg(sim.dam.name)
                    .arg(slice->frontDistanceKm, 0, 'f', 1)
                    .arg(sim.currentMinute);

                topTooltips.push_back([frontScreen, frontBadge](QPainter& p) {
                    QFont fFont("Segoe UI", 7, QFont::DemiBold);
                    p.setFont(fFont);
                    QFontMetricsF ffm(fFont);
                    QRectF bRect(frontScreen.x() - ffm.horizontalAdvance(frontBadge) / 2.0 - 6,
                                 frontScreen.y() - ffm.height() - 14,
                                 ffm.horizontalAdvance(frontBadge) + 12, ffm.height() + 4);
                    p.setBrush(QColor(24, 24, 27, 255));
                    p.setPen(QPen(QColor(63, 63, 70, 255), 1.0));
                    p.drawRoundedRect(bRect, 4.0, 4.0);
                    p.setPen(QColor(56, 189, 248));
                    p.drawText(bRect, Qt::AlignCenter, frontBadge);

                    QPolygonF pointerArrow;
                    pointerArrow << QPointF(frontScreen.x() - 4, bRect.bottom())
                                 << QPointF(frontScreen.x() + 4, bRect.bottom())
                                 << QPointF(frontScreen.x(), frontScreen.y() - 1);
                    p.setBrush(QColor(24, 24, 27, 255));
                    p.setPen(QPen(QColor(63, 63, 70, 255), 1.0));
                    p.drawPolygon(pointerArrow);
                });
            }
        }

        // 6. Downstream Danger Zones
        if (showDetailTooltips) {
            for (const auto& dz : sim.dangerZones) {
                QPointF dzScreen = toCanvasPoint(dz.lat, dz.lon);
                bool isInundated = (slice->frontDistanceKm >= dz.distanceKm && sim.currentMinute > 0);

                double pitRadiusLatDeg = 0.0035 + 0.0005 * std::clamp(dz.peakDepthM, 1.0, 10.0);
                double pitRadiusLonDeg = 0.0050 + 0.0007 * std::clamp(dz.peakDepthM, 1.0, 10.0);
                QPolygonF pitCanvasPoly;
                int numVerts = 14;
                for (int v = 0; v < numVerts; ++v) {
                    double theta = (2.0 * M_PI * v) / numVerts;
                    double noise = 0.82 + 0.28 * std::sin(theta * 3.0 + dz.distanceKm * 0.75);
                    double vLat = dz.lat + pitRadiusLatDeg * noise * std::cos(theta);
                    double vLon = dz.lon + pitRadiusLonDeg * noise * std::sin(theta);
                    pitCanvasPoly.append(toCanvasPoint(vLat, vLon));
                }

                if (pitCanvasPoly.size() >= 3) {
                    painter.setBrush(QColor(244, 63, 94, isInundated ? 60 : 35));
                    painter.setPen(QPen(QColor(251, 113, 133, isInundated ? 170 : 90), 1.4, Qt::DashLine));
                    painter.drawPolygon(pitCanvasPoly);

                    QTransform tMid;
                    tMid.translate(dzScreen.x(), dzScreen.y());
                    tMid.scale(0.65, 0.65);
                    tMid.translate(-dzScreen.x(), -dzScreen.y());
                    QPolygonF midDangerPoly = tMid.map(pitCanvasPoly);
                    painter.setBrush(QColor(225, 29, 72, isInundated ? 110 : 65));
                    painter.setPen(Qt::NoPen);
                    painter.drawPolygon(midDangerPoly);

                    QTransform tCore;
                    tCore.translate(dzScreen.x(), dzScreen.y());
                    tCore.scale(0.35, 0.35);
                    tCore.translate(-dzScreen.x(), -dzScreen.y());
                    QPolygonF coreDangerPoly = tCore.map(pitCanvasPoly);
                    painter.setBrush(QColor(159, 18, 57, isInundated ? 160 : 95));
                    painter.drawPolygon(coreDangerPoly);
                }

                double dzPulse = std::fmod((flowAnimPhase * 0.85), 16.0);
                painter.setPen(QPen(QColor(244, 63, 94, isInundated ? std::max(0, 240 - static_cast<int>(dzPulse * 12)) : 60), 1.5));
                painter.setBrush(QColor(244, 63, 94, isInundated ? std::max(0, 70 - static_cast<int>(dzPulse * 4)) : 15));
                painter.drawEllipse(dzScreen, 6.0 + dzPulse, 6.0 + dzPulse);

                painter.setPen(QPen(Qt::white, 1.5));
                painter.setBrush(isInundated ? QColor(225, 29, 72) : QColor(244, 63, 94));
                painter.drawEllipse(dzScreen, 4.0, 4.0);

                if (zoomLevel >= 8) {
                    QString dzLabel = QString("⚠️ %1 (%2 km · %3)")
                        .arg(dz.name)
                        .arg(dz.distanceKm, 0, 'f', 1)
                        .arg(isInundated ? QString("INUNDATED %1m").arg(dz.peakDepthM, 0, 'f', 1) : QString("ETA %1m").arg(qRound(dz.arrivalTimeMin)));

                    topTooltips.push_back([dzScreen, dzLabel, isInundated](QPainter& p) {
                        QFont dzFont("Segoe UI", 7, QFont::DemiBold);
                        p.setFont(dzFont);
                        QFontMetricsF mfm(dzFont);
                        QRectF dRect(dzScreen.x() + 8, dzScreen.y() - mfm.height() / 2.0 - 1, mfm.horizontalAdvance(dzLabel) + 10, mfm.height() + 4);
                        p.setBrush(QColor(24, 24, 27, 255));
                        p.setPen(QPen(isInundated ? QColor(244, 63, 94, 230) : QColor(251, 113, 133, 200), 1.0));
                        p.drawRoundedRect(dRect, 4.0, 4.0);
                        p.setPen(isInundated ? QColor(254, 205, 211) : QColor(244, 244, 245));
                        p.drawText(dRect, Qt::AlignCenter, dzLabel);
                    });
                }
            }
        }
    };

    if (hydroFlowMode) {
        // Multi-dam Hydro Flow mode: Render every dam's hydrodynamic simulation
        for (const auto& sim : hydroFlowSimulations) {
            renderSingleFloodSimulation(sim, zoomLevel >= 7);
        }
    } else if (floodSimulation.isActive) {
        // Single dam simulation mode
        renderSingleFloodSimulation(floodSimulation, true);
    }
    // --- Render Open-Meteo Weather Forecast Visual Overlay (Cloud Cover, Precipitation Radar, Wind Streamlines) ---
    if (weatherMode && weatherForecast.isValid) {
        const auto* hw = weatherForecast.getHour(weatherHourIndex);
        if (hw) {
            auto toCanvasPoint = [&](double lat, double lon) -> QPointF {
                return geoToScreen(lat, lon);
            };

            // 1. Cloud Cover Atmosphere Wash Overlay
            if (hw->cloudCoverPct > 1.0) {
                int cloudAlpha = std::clamp(static_cast<int>((hw->cloudCoverPct / 100.0) * 85.0), 10, 85);
                painter.fillRect(rect(), QColor(241, 245, 249, cloudAlpha));
            }

            // 2. Precipitation Radar & Animated Raindrop Vectors
            if (hw->precipitationMm > 0.0) {
                int precipAlpha = std::clamp(static_cast<int>(hw->precipitationMm * 20.0), 25, 95);
                QColor radarColor = (hw->precipitationMm > 5.0) ? QColor(225, 29, 72, precipAlpha)
                                  : (hw->precipitationMm > 2.0 ? QColor(37, 99, 235, precipAlpha)
                                  : QColor(52, 211, 153, precipAlpha));
                painter.fillRect(rect(), radarColor);

                // Animated rain streak lines across canvas
                int numStreaks = std::clamp(static_cast<int>(hw->precipitationMm * 15.0) + 20, 20, 80);
                painter.setPen(QPen(QColor(186, 230, 253, 140), 1.2, Qt::SolidLine, Qt::RoundCap));
                for (int s = 0; s < numStreaks; ++s) {
                    double sx = std::fmod((s * 47.0 + flowAnimPhase * 4.0), static_cast<double>(width()));
                    double sy = std::fmod((s * 73.0 + flowAnimPhase * 12.0), static_cast<double>(height()));
                    double slant = 4.0 + (hw->windSpeedKmh * 0.4);
                    painter.drawLine(QPointF(sx, sy), QPointF(sx + slant, sy + 14.0));
                }
            }

            // 3. Wind Streamlines
            if (hw->windSpeedKmh > 2.0) {
                int numWindLines = 8;
                painter.setPen(QPen(QColor(253, 224, 71, 70), 1.0, Qt::DashLine));
                for (int w = 0; w < numWindLines; ++w) {
                    double wy = (height() / (numWindLines + 1)) * (w + 1);
                    double wxOffset = std::fmod(flowAnimPhase * (hw->windSpeedKmh * 0.8) + w * 90.0, static_cast<double>(width()));
                    painter.drawLine(QPointF(wxOffset, wy), QPointF(wxOffset + 60.0, wy + 4.0));
                }
            }

            // 4. Target Pinpoint at Forecast Coordinate
            QPointF targetScreen = toCanvasPoint(weatherForecast.latitude, weatherForecast.longitude);

            // Pulsing Emerald Beacon
            double wPulse = std::fmod((flowAnimPhase * 0.8), 16.0);
            painter.setPen(QPen(QColor(52, 211, 153, std::max(0, 200 - static_cast<int>(wPulse * 10))), 1.8));
            painter.setBrush(QColor(52, 211, 153, 40));
            painter.drawEllipse(targetScreen, 8.0 + wPulse, 8.0 + wPulse);

            painter.setPen(QPen(Qt::white, 1.4));
            painter.setBrush(QColor(52, 211, 153));
            painter.drawEllipse(targetScreen, 5.0, 5.0);

            // Top Z-Index Floating HUD Badge
            QString wBadge = QString("📍 %1 · %2°C · Rain: %3mm · Wind: %4km/h")
                                .arg(weatherForecast.locationName)
                                .arg(hw->temperatureC, 0, 'f', 1)
                                .arg(hw->precipitationMm, 0, 'f', 1)
                                .arg(hw->windSpeedKmh, 0, 'f', 1);

            topTooltips.push_back([targetScreen, wBadge](QPainter& p) {
                QFont wFont("Segoe UI", 8, QFont::DemiBold);
                p.setFont(wFont);
                QFontMetricsF wfm(wFont);
                double bw = wfm.horizontalAdvance(wBadge) + 16.0;
                double bh = wfm.height() + 6.0;
                QRectF wRect(targetScreen.x() - bw / 2.0, targetScreen.y() - bh - 12.0, bw, bh);

                p.setBrush(QColor(24, 24, 27, 255));
                p.setPen(QPen(QColor(52, 211, 153), 1.2));
                p.drawRoundedRect(wRect, 5.0, 5.0);

                p.setPen(QColor(244, 244, 245));
                p.drawText(wRect, Qt::AlignCenter, wBadge);

                // Downward pointer
                QPolygonF pointer;
                pointer << QPointF(targetScreen.x() - 4, wRect.bottom())
                        << QPointF(targetScreen.x() + 4, wRect.bottom())
                        << QPointF(targetScreen.x(), targetScreen.y() - 1);
                p.setBrush(QColor(24, 24, 27, 255));
                p.setPen(QPen(QColor(52, 211, 153), 1.0));
                p.drawPolygon(pointer);
            });
        }
    }

    // -------------------------------------------------------------
    // Helicopter Live SAR Layer (Glowing Dots + Vectors + Breadcrumbs)
    // -------------------------------------------------------------
    if (showHelicopters || !liveHelicopters.empty()) {
        auto geoToCanvasPoint = [&](double lat, double lon) -> QPointF {
            double tX = lonToTileX(lon, zoomLevel);
            double tY = latToTileY(lat, zoomLevel);
            return QPointF(cx + (tX - centerTileX) * TILE_SIZE, cy + (tY - centerTileY) * TILE_SIZE);
        };

        for (const auto& heli : liveHelicopters) {
            QPointF hScreen = geoToCanvasPoint(heli.lat, heli.lon);
            if (hScreen.x() < -80 || hScreen.x() > w + 80 || hScreen.y() < -80 || hScreen.y() > h + 80) {
                continue;
            }

            bool isSelected = (heli.hex == selectedHelicopterHex);

            // 1. Breadcrumb Trail History
            if (heli.trailHistory.size() > 1) {
                QPainterPath trailPath;
                bool first = true;
                for (const auto& pt : heli.trailHistory) {
                    QPointF tp = geoToCanvasPoint(pt.y(), pt.x()); // pt.y() is lat, pt.x() is lon
                    if (first) {
                        trailPath.moveTo(tp);
                        first = false;
                    } else {
                        trailPath.lineTo(tp);
                    }
                }
                QColor trailColor = isSelected ? QColor(6, 182, 212, 180) : QColor(245, 158, 11, 120);
                painter.setPen(QPen(trailColor, isSelected ? 2.0 : 1.4, Qt::DashLine, Qt::RoundCap));
                painter.drawPath(trailPath);
            }

            // 2. Velocity / Heading Vector
            double rad = (heli.trackHeading - 90.0) * M_PI / 180.0;
            double speedFactor = std::clamp(heli.groundSpeedKnots / 8.0, 14.0, 40.0);
            QPointF headEnd(hScreen.x() + std::cos(rad) * speedFactor, hScreen.y() + std::sin(rad) * speedFactor);
            painter.setPen(QPen(isSelected ? QColor(6, 182, 212, 240) : QColor(251, 191, 36, 220), 2.0, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(hScreen, headEnd);

            // 3. Spinning Helicopter Rotor Blades (4 Animated Blades)
            double rotorAngle = (flowAnimPhase * 25.0) * M_PI / 180.0;
            double bladeLen = isSelected ? 12.0 : 9.0;
            painter.setPen(QPen(isSelected ? QColor(6, 182, 212, 180) : QColor(251, 191, 36, 160), 1.4, Qt::SolidLine, Qt::RoundCap));
            for (int b = 0; b < 4; ++b) {
                double bRad = rotorAngle + (b * M_PI / 2.0);
                painter.drawLine(hScreen, QPointF(hScreen.x() + std::cos(bRad) * bladeLen, hScreen.y() + std::sin(bRad) * bladeLen));
            }

            // 4. Pulsing Radar Dot Ring & Fluid Wash
            double pulse = std::fmod(flowAnimPhase * 0.9, 16.0);
            painter.setPen(QPen(isSelected ? QColor(6, 182, 212, std::max(0, 240 - static_cast<int>(pulse * 14)))
                                           : QColor(245, 158, 11, std::max(0, 200 - static_cast<int>(pulse * 12))), 1.6));
            painter.setBrush(isSelected ? QColor(6, 182, 212, 40) : QColor(245, 158, 11, 30));
            painter.drawEllipse(hScreen, 6.0 + pulse, 6.0 + pulse);

            // 5. Solid Central Helicopter Core Dot
            painter.setPen(QPen(QColor(18, 18, 22), 1.8));
            painter.setBrush(isSelected ? QColor(6, 182, 212) : QColor(245, 158, 11)); // Cyan if selected, Amber if active
            painter.drawEllipse(hScreen, 6.0, 6.0);

            // 6. Callsign / Altitude Badge (Tooltip Layer without icon)
            QString heliLabel = QString("%1 · %2ft · %3kt")
                                    .arg(heli.flight.isEmpty() ? heli.registration : heli.flight)
                                    .arg(static_cast<int>(heli.altitudeFt))
                                    .arg(static_cast<int>(heli.groundSpeedKnots));

            topTooltips.push_back([hScreen, heliLabel, isSelected](QPainter& p) {
                QFont hFont("Segoe UI", isSelected ? 8 : 7, isSelected ? QFont::Bold : QFont::DemiBold);
                p.setFont(hFont);
                QFontMetricsF hfm(hFont);
                double bw = hfm.horizontalAdvance(heliLabel) + 12.0;
                double bh = hfm.height() + 4.0;
                QRectF hRect(hScreen.x() - bw / 2.0, hScreen.y() - bh - 10.0, bw, bh);

                p.setBrush(QColor(18, 18, 22, 245));
                p.setPen(QPen(isSelected ? QColor(6, 182, 212) : QColor(245, 158, 11), isSelected ? 1.5 : 1.0));
                p.drawRoundedRect(hRect, 4.0, 4.0);

                p.setPen(isSelected ? QColor(255, 255, 255) : QColor(244, 244, 245));
                p.drawText(hRect, Qt::AlignCenter, heliLabel);
            });
        }
    }

    // --- Render All Tooltips & Badges at Highest Z-Index (Above all paths, dots, and polygons) ---
    for (const auto& renderTooltip : topTooltips) {
        renderTooltip(painter);
    }

    // End Rotated Map Scene
    painter.restore();

    // 2. Draw Upright Overlays & Dynamic Axis Crosshair (Screen Middle Plus)
    // Determine dynamic contrast color based on background tile luminance at center
    bool isDarkBg = true;
    QPixmap* centerTile = getTile(zoomLevel, centerTileXInt, centerTileYInt);
    if (centerTile && !centerTile->isNull()) {
        int px = std::clamp(static_cast<int>((centerTileX - std::floor(centerTileX)) * TILE_SIZE), 0, TILE_SIZE - 1);
        int py = std::clamp(static_cast<int>((centerTileY - std::floor(centerTileY)) * TILE_SIZE), 0, TILE_SIZE - 1);
        QImage tileImg = centerTile->toImage();
        if (px >= 0 && px < tileImg.width() && py >= 0 && py < tileImg.height()) {
            QColor centerPixel = tileImg.pixelColor(px, py);
            double luminance = 0.299 * centerPixel.red() + 0.587 * centerPixel.green() + 0.114 * centerPixel.blue();
            isDarkBg = (luminance < 135.0);
        }
    }

    // Dynamic contrast plus sign: White on dark backgrounds, Dark zinc on light/white backgrounds
    QColor plusCoreColor = isDarkBg ? QColor(255, 255, 255, 250) : QColor(20, 20, 24, 250);
    QColor plusShadowColor = isDarkBg ? QColor(0, 0, 0, 180) : QColor(255, 255, 255, 180);

    // Outer contrast outline / shadow (for 100% visibility over any surface texture)
    painter.setPen(QPen(plusShadowColor, 3.2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(cx - 8, cy, cx + 8, cy);
    painter.drawLine(cx, cy - 8, cx, cy + 8);

    // Inner dynamic core stroke
    painter.setPen(QPen(plusCoreColor, 1.6, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(cx - 8, cy, cx + 8, cy);
    painter.drawLine(cx, cy - 8, cx, cy + 8);

    // Rotation angle badge (if rotated)
    if (std::abs(rotationAngle) > 0.5) {
        QString rotText = QString("🧭 %1°").arg(qRound(rotationAngle));
        QFont rotFont("Segoe UI", 9, QFont::Bold);
        painter.setFont(rotFont);
        QFontMetricsF rfm(rotFont);
        QRectF rotRect(w - rfm.horizontalAdvance(rotText) - 30, 16, rfm.horizontalAdvance(rotText) + 16, rfm.height() + 6);
        painter.setBrush(QColor(20, 20, 24, 230));
        painter.setPen(QPen(QColor(84, 213, 154, 200), 1.2));
        painter.drawRoundedRect(rotRect, 5.0, 5.0);
        painter.setPen(QColor(84, 213, 154));
        painter.drawText(rotRect, Qt::AlignCenter, rotText);
    }

    // Measure instructions HUD hint in top-center
    if (measureMode && measurePoints.empty()) {
        QString hint = "Click anywhere on map to measure distance (Right-click or ESC to exit)";
        QFont hFont("Segoe UI", 9, QFont::Medium);
        painter.setFont(hFont);
        QFontMetricsF hfm(hFont);
        QRectF hRect((w - hfm.horizontalAdvance(hint) - 20) / 2.0, 16, hfm.horizontalAdvance(hint) + 20, hfm.height() + 8);
        painter.setBrush(QColor(20, 20, 24, 230));
        painter.setPen(QPen(QColor(138, 180, 248, 160), 1.0));
        painter.drawRoundedRect(hRect, 6, 6);
        painter.setPen(QColor(230, 230, 235));
        painter.drawText(hRect, Qt::AlignCenter, hint);
    }

    // Required Attribution text
    painter.setPen(QColor(230, 230, 230));
    QFont attrFont("Segoe UI", 9, QFont::Medium);
    painter.setFont(attrFont);
    QString attribution = "© OpenStreetMap contributors";
    QRect attrRect(w - 240, h - 26, 230, 20);
    painter.fillRect(attrRect.adjusted(-6, -2, 6, 2), QColor(20, 20, 22, 220));
    painter.drawText(attrRect, Qt::AlignRight | Qt::AlignVCenter, attribution);
}

// --- Coordinate Projection Helpers & Distance ---

QPointF OnlineTileWidget::unrotatePoint(const QPointF& pt) const {
    if (std::abs(rotationAngle) < 0.001) return pt;
    double rad = -rotationAngle * M_PI / 180.0;
    double dx = pt.x() - width() / 2.0;
    double dy = pt.y() - height() / 2.0;
    double rx = dx * std::cos(rad) - dy * std::sin(rad);
    double ry = dx * std::sin(rad) + dy * std::cos(rad);
    return QPointF(width() / 2.0 + rx, height() / 2.0 + ry);
}

QPointF OnlineTileWidget::rotatePoint(const QPointF& pt) const {
    if (std::abs(rotationAngle) < 0.001) return pt;
    double rad = rotationAngle * M_PI / 180.0;
    double dx = pt.x() - width() / 2.0;
    double dy = pt.y() - height() / 2.0;
    double rx = dx * std::cos(rad) - dy * std::sin(rad);
    double ry = dx * std::sin(rad) + dy * std::cos(rad);
    return QPointF(width() / 2.0 + rx, height() / 2.0 + ry);
}

double OnlineTileWidget::haversineDistanceM(double lat1, double lon1, double lat2, double lon2) {
    constexpr double R = 6371008.8; // Earth's mean radius in meters
    double dLat = (lat2 - lat1) * M_PI / 180.0;
    double dLon = (lon2 - lon1) * M_PI / 180.0;
    double a = std::sin(dLat / 2.0) * std::sin(dLat / 2.0) +
               std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
               std::sin(dLon / 2.0) * std::sin(dLon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return R * c;
}

double OnlineTileWidget::screenToLon(double screenX, double screenY) const {
    QPointF unrot = unrotatePoint(QPointF(screenX, screenY));
    double centerTileX = lonToTileX(centerLon, zoomLevel);
    double tileX = centerTileX + (unrot.x() - width() / 2.0) / TILE_SIZE;
    return tileXToLon(tileX, zoomLevel);
}

double OnlineTileWidget::screenToLat(double screenX, double screenY) const {
    QPointF unrot = unrotatePoint(QPointF(screenX, screenY));
    double centerTileY = latToTileY(centerLat, zoomLevel);
    double tileY = centerTileY + (unrot.y() - height() / 2.0) / TILE_SIZE;
    return tileYToLat(tileY, zoomLevel);
}

QPointF OnlineTileWidget::geoToScreen(double lat, double lon) const {
    double centerTileX = lonToTileX(centerLon, zoomLevel);
    double centerTileY = latToTileY(centerLat, zoomLevel);
    double tileX = lonToTileX(lon, zoomLevel);
    double tileY = latToTileY(lat, zoomLevel);
    double sx = width() / 2.0 + (tileX - centerTileX) * TILE_SIZE;
    double sy = height() / 2.0 + (tileY - centerTileY) * TILE_SIZE;
    return rotatePoint(QPointF(sx, sy));
}

void OnlineTileWidget::setTool(MapTool tool) {
    currentTool = tool;
    if (tool == MapTool::Move) {
        setMeasureMode(false);
        clearBoxSelection();
        setCursor(Qt::ArrowCursor);
    } else if (tool == MapTool::Select) {
        setMeasureMode(false);
        setCursor(Qt::CrossCursor);
    } else if (tool == MapTool::Rotate) {
        setMeasureMode(false);
        clearBoxSelection();
        setCursor(Qt::SizeAllCursor);
    } else if (tool == MapTool::Ruler) {
        clearBoxSelection();
        setMeasureMode(true);
    }
    update();
}

void OnlineTileWidget::setFloodSimulation(const MapCore::FloodSimulationState& sim) {
    floodSimulation = sim;
    update();
}

void OnlineTileWidget::setHydroFlowMode(bool active) {
    hydroFlowMode = active;
    if (active) {
        if (hydroFlowSimulations.empty() && damManager && damManager->hasData()) {
            const auto& allDams = damManager->getDams();
            hydroFlowSimulations.reserve(allDams.size());
            for (const auto& dam : allDams) {
                hydroFlowSimulations.push_back(MapCore::DamFloodSimulator::compute60MinSimulation(dam));
            }
        }
        for (auto& sim : hydroFlowSimulations) {
            sim.currentMinute = currentSimulationMinute;
        }
    }
    update();
}

void OnlineTileWidget::updateFloodSimulationMinute(int minute) {
    currentSimulationMinute = std::clamp(minute, 0, 60);
    if (floodSimulation.isActive) {
        floodSimulation.currentMinute = currentSimulationMinute;
    }
    if (hydroFlowMode) {
        for (auto& sim : hydroFlowSimulations) {
            sim.currentMinute = currentSimulationMinute;
        }
    }
    update();
}

void OnlineTileWidget::clearFloodSimulation() {
    floodSimulation.isActive = false;
    floodSimulation.timeSlices.clear();
    floodSimulation.rawNodes.clear();
    update();
}

void OnlineTileWidget::setRotation(double degrees) {
    rotationAngle = degrees;
    while (rotationAngle >= 360.0) rotationAngle -= 360.0;
    while (rotationAngle < 0.0) rotationAngle += 360.0;
    emit rotationChanged(rotationAngle);
    update();
}

void OnlineTileWidget::resetRotation() {
    setRotation(0.0);
}

void OnlineTileWidget::setMeasureMode(bool active) {
    measureMode = active;
    if (!active) {
        clearMeasure();
    }
    setCursor(active ? Qt::CrossCursor : (currentTool == MapTool::Select ? Qt::CrossCursor : (currentTool == MapTool::Rotate ? Qt::SizeAllCursor : Qt::ArrowCursor)));
    emit measureModeChanged(active);
    update();
}

void OnlineTileWidget::clearMeasure() {
    measurePoints.clear();
    hasLiveMouse = false;
    update();
}

void OnlineTileWidget::clearBoxSelection() {
    isBoxSelecting = false;
    selectedDams.clear();
    update();
}

// --- Mouse interaction ---

void OnlineTileWidget::mousePressEvent(QMouseEvent* event) {
    pressMousePos = event->pos();

    // Middle Mouse Button (Wheel Press): Pan/Move map seamlessly across all active tools
    if (event->button() == Qt::MiddleButton) {
        isMiddleDragging = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (currentTool == MapTool::Rotate) {
            isRotating = true;
            double dx = event->pos().x() - width() / 2.0;
            double dy = event->pos().y() - height() / 2.0;
            lastRotationMouseAngle = std::atan2(dy, dx) * 180.0 / M_PI;
            update();
        } else if (currentTool == MapTool::Select) {
            isBoxSelecting = true;
            boxSelectStart = event->pos();
            boxSelectCurrent = event->pos();
            isDragging = false;
            update();
        } else {
            isDragging = true;
            lastMousePos = event->pos();
            if (!measureMode) {
                setCursor(Qt::ClosedHandCursor);
            }
        }
    } else if (event->button() == Qt::RightButton) {
        if (measureMode) {
            if (!measurePoints.empty()) {
                measurePoints.pop_back();
                update();
            } else {
                setMeasureMode(false);
            }
        } else if (isBoxSelecting) {
            clearBoxSelection();
        } else {
            emit contextMenuRequested(event->globalPosition().toPoint());
        }
    }
}

void OnlineTileWidget::mouseMoveEvent(QMouseEvent* event) {
    liveMousePos = event->pos();
    hasLiveMouse = true;

    // 1. Middle Mouse Pan (or Left Mouse Drag in Move tool)
    if ((isMiddleDragging && (event->buttons() & Qt::MiddleButton)) ||
        (isDragging && (event->buttons() & Qt::LeftButton))) {
        QPoint delta = event->pos() - lastMousePos;
        lastMousePos = event->pos();

        // Compensate for viewport rotation when panning
        double rad = -rotationAngle * M_PI / 180.0;
        double rdx = delta.x() * std::cos(rad) - delta.y() * std::sin(rad);
        double rdy = delta.x() * std::sin(rad) + delta.y() * std::cos(rad);

        double tileCount = std::pow(2.0, zoomLevel);
        double lonDelta = -rdx / (TILE_SIZE * tileCount) * 360.0;

        double centerTileY = latToTileY(centerLat, zoomLevel);
        double newTileY = centerTileY - rdy / static_cast<double>(TILE_SIZE);
        double newLat = tileYToLat(newTileY, zoomLevel);

        centerLon = std::clamp(centerLon + lonDelta, -180.0, 180.0);
        centerLat = std::clamp(newLat, -85.0511, 85.0511);

        emitViewportChanged();
        update();
        return;
    }

    if (isRotating && (event->buttons() & Qt::LeftButton)) {
        double dx = event->pos().x() - width() / 2.0;
        double dy = event->pos().y() - height() / 2.0;
        double curAngle = std::atan2(dy, dx) * 180.0 / M_PI;
        double delta = curAngle - lastRotationMouseAngle;
        while (delta > 180.0) delta -= 360.0;
        while (delta < -180.0) delta += 360.0;

        rotationAngle += delta;
        while (rotationAngle >= 360.0) rotationAngle -= 360.0;
        while (rotationAngle < 0.0) rotationAngle += 360.0;

        lastRotationMouseAngle = curAngle;
        emit rotationChanged(rotationAngle);
        update();
        return;
    }

    if (isBoxSelecting && (event->buttons() & Qt::LeftButton)) {
        boxSelectCurrent = event->pos();
        update();
        return;
    }

    if (measureMode) {
        update();
    }
}

void OnlineTileWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        isMiddleDragging = false;
        setCursor((measureMode || currentTool == MapTool::Select) ? Qt::CrossCursor : (currentTool == MapTool::Rotate ? Qt::SizeAllCursor : Qt::ArrowCursor));
        update();
        return;
    }

    if (event->button() == Qt::LeftButton) {
        if (isRotating) {
            isRotating = false;
            setCursor(Qt::SizeAllCursor);
            return;
        }

        if (isBoxSelecting) {
            isBoxSelecting = false;
            QPointF unrotStart = unrotatePoint(boxSelectStart);
            QPointF unrotCurrent = unrotatePoint(boxSelectCurrent);
            QRectF selRect = QRectF(unrotStart, unrotCurrent).normalized();

            if (selRect.width() > 6 || selRect.height() > 6) {
                double minLon = screenToLon(boxSelectStart.x(), boxSelectStart.y());
                double maxLon = screenToLon(boxSelectCurrent.x(), boxSelectCurrent.y());
                double minLat = screenToLat(boxSelectStart.x(), boxSelectStart.y());
                double maxLat = screenToLat(boxSelectCurrent.x(), boxSelectCurrent.y());

                if (minLon > maxLon) std::swap(minLon, maxLon);
                if (minLat > maxLat) std::swap(minLat, maxLat);

                selectedDams.clear();
                if (damManager && showDams && damManager->hasData()) {
                    damManager->getDamsInBbox(minLat, minLon, maxLat, maxLon, selectedDams);
                }

                emit boxSelectionCompleted(minLat, minLon, maxLat, maxLon, static_cast<int>(selectedDams.size()));
                update();
            } else {
                // Point click inspection
                double clickLat = screenToLat(event->pos().x(), event->pos().y());
                double clickLon = screenToLon(event->pos().x(), event->pos().y());
                if (damManager && showDams && damManager->hasData()) {
                    const auto* nearest = damManager->findNearest(clickLat, clickLon, 0.08);
                    if (nearest) {
                        selectedDams.clear();
                        selectedDams.push_back(nearest);
                        emit damClicked(*nearest);
                        update();
                    } else {
                        clearBoxSelection();
                    }
                }
            }
            return;
        }

        isDragging = false;
        setCursor((measureMode || currentTool == MapTool::Select) ? Qt::CrossCursor : (currentTool == MapTool::Rotate ? Qt::SizeAllCursor : Qt::ArrowCursor));

        // Check if it was a clean stationary click without dragging
        if ((event->pos() - pressMousePos).manhattanLength() < 6) {
            // 1. First check if any Helicopter dot was clicked
            if (showHelicopters || !liveHelicopters.empty()) {
                auto geoToCanvasPoint = [this](double lat, double lon) -> QPointF {
                    double tX = lonToTileX(lon, zoomLevel);
                    double tY = latToTileY(lat, zoomLevel);
                    double curTileX = lonToTileX(centerLon, zoomLevel);
                    double curTileY = latToTileY(centerLat, zoomLevel);
                    return QPointF((width() / 2.0) + (tX - curTileX) * TILE_SIZE, (height() / 2.0) + (tY - curTileY) * TILE_SIZE);
                };

                QPointF unrotClick = unrotatePoint(event->pos());
                for (const auto& heli : liveHelicopters) {
                    QPointF hScreen = geoToCanvasPoint(heli.lat, heli.lon);
                    double dist = std::hypot(unrotClick.x() - hScreen.x(), unrotClick.y() - hScreen.y());
                    if (dist <= 20.0) {
                        selectedHelicopterHex = heli.hex;
                        emit helicopterClicked(heli);
                        update();
                        return;
                    }
                }
            }

            double clickLat = screenToLat(event->pos().x(), event->pos().y());
            double clickLon = screenToLon(event->pos().x(), event->pos().y());

            if (measureMode) {
                measurePoints.emplace_back(clickLat, clickLon);
                update();
            } else if (damManager && showDams && damManager->hasData()) {
                const auto* nearest = damManager->findNearest(clickLat, clickLon, 0.08);
                if (nearest) {
                    emit damClicked(*nearest);
                }
            }
        }
    }
}

void OnlineTileWidget::leaveEvent(QEvent* /*event*/) {
    hasLiveMouse = false;
    if (measureMode) {
        update();
    }
}

void OnlineTileWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !measureMode) {
        // Zoom into clicked point
        double curTileX = lonToTileX(centerLon, zoomLevel);
        double curTileY = latToTileY(centerLat, zoomLevel);

        double clickTileX = curTileX + (event->pos().x() - width() / 2.0) / TILE_SIZE;
        double clickTileY = curTileY + (event->pos().y() - height() / 2.0) / TILE_SIZE;

        setCenter(tileYToLat(clickTileY, zoomLevel), tileXToLon(clickTileX, zoomLevel));
        zoomIn();
    }
}

void OnlineTileWidget::wheelEvent(QWheelEvent* event) {
    double rawDelta = event->angleDelta().y();
    if (invertScroll) rawDelta = -rawDelta;

    wheelAccumulator += rawDelta * zoomSensitivity;

    // Smooth threshold for mouse notches and continuous two-finger touchpads
    const double threshold = 90.0;

    if (std::abs(wheelAccumulator) >= threshold) {
        int steps = static_cast<int>(wheelAccumulator / threshold);
        wheelAccumulator -= steps * threshold;

        if (anchorZoomToCursor) {
            QPointF mousePos = event->position();
            double curTileX = lonToTileX(centerLon, zoomLevel);
            double curTileY = latToTileY(centerLat, zoomLevel);

            double mouseTileX = curTileX + (mousePos.x() - width() / 2.0) / TILE_SIZE;
            double mouseTileY = curTileY + (mousePos.y() - height() / 2.0) / TILE_SIZE;
            double mouseLon = tileXToLon(mouseTileX, zoomLevel);
            double mouseLat = tileYToLat(mouseTileY, zoomLevel);

            int newZoom = std::clamp(zoomLevel + steps, 2, 19);
            if (newZoom != zoomLevel) {
                zoomLevel = newZoom;
                double newMouseTileX = lonToTileX(mouseLon, zoomLevel);
                double newMouseTileY = latToTileY(mouseLat, zoomLevel);
                double newCenterTileX = newMouseTileX - (mousePos.x() - width() / 2.0) / TILE_SIZE;
                double newCenterTileY = newMouseTileY - (mousePos.y() - height() / 2.0) / TILE_SIZE;
                centerLon = std::clamp(tileXToLon(newCenterTileX, zoomLevel), -180.0, 180.0);
                centerLat = std::clamp(tileYToLat(newCenterTileY, zoomLevel), -85.0511, 85.0511);
                emitViewportChanged();
                update();
            }
        } else {
            setZoom(zoomLevel + steps);
        }
    }
}

void OnlineTileWidget::keyPressEvent(QKeyEvent* event) {
    if (measureMode) {
        if (event->key() == Qt::Key_Escape) {
            setMeasureMode(false);
            return;
        } else if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
            if (!measurePoints.empty()) {
                measurePoints.pop_back();
                update();
                return;
            }
        }
    }

    double tileCount = std::pow(2.0, zoomLevel);
    double stepDeg = 60.0 / (TILE_SIZE * tileCount) * 360.0;

    switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_A:
            setCenter(centerLat, centerLon - stepDeg);
            break;
        case Qt::Key_Right:
        case Qt::Key_D:
            setCenter(centerLat, centerLon + stepDeg);
            break;
        case Qt::Key_Up:
        case Qt::Key_W:
            setCenter(centerLat + stepDeg, centerLon);
            break;
        case Qt::Key_Down:
        case Qt::Key_S:
            setCenter(centerLat - stepDeg, centerLon);
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn();
            break;
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            zoomOut();
            break;
        case Qt::Key_Home:
        case Qt::Key_0:
            fitIndia();
            break;
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

void OnlineTileWidget::resizeEvent(QResizeEvent* /*event*/) {
    update();
}

} // namespace MapUI
