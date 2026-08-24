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

OnlineTileWidget::OnlineTileWidget(QWidget* parent) : QWidget(parent), tileCache(500) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    networkManager = new QNetworkAccessManager(this);

    // Continuous 30 FPS fluid animation timer for river flow & ripple dynamics
    flowAnimTimer = new QTimer(this);
    flowAnimTimer->setInterval(33);
    connect(flowAnimTimer, &QTimer::timeout, this, [this]() {
        if (floodSimulation.isActive) {
            flowAnimPhase = (flowAnimPhase + 1) % 10000;
            update();
        }
    });

    // Initial center on India
    centerLat = 22.0;
    centerLon = 79.0;
    zoomLevel = 5;
}

void OnlineTileWidget::setCenter(double lat, double lon) {
    centerLat = std::clamp(lat, -85.0511, 85.0511);
    centerLon = std::clamp(lon, -180.0, 180.0);
    emitViewportChanged();
    update();
}

void OnlineTileWidget::setZoom(int z) {
    zoomLevel = std::clamp(z, 2, 19);
    emitViewportChanged();
    update();
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

// --- Tile URL Builders ---

QString OnlineTileWidget::getPrimaryUrl(OnlineTileProvider provider, int zoom, int x, int y) const {
    switch (provider) {
        case OnlineTileProvider::OpenStreetMap_Standard:
            return QString("https://tile.openstreetmap.org/%1/%2/%3.png").arg(zoom).arg(x).arg(y);
        case OnlineTileProvider::OpenStreetMap_DE:
            return QString("https://tile.openstreetmap.de/%1/%2/%3.png").arg(zoom).arg(x).arg(y);
        case OnlineTileProvider::OpenStreetMap_Voyager:
            return QString("https://a.basemaps.cartocdn.com/rastertiles/voyager/%1/%2/%3.png").arg(zoom).arg(x).arg(y);
        case OnlineTileProvider::OpenStreetMap_Dark:
            return QString("https://a.basemaps.cartocdn.com/dark_all/%1/%2/%3.png").arg(zoom).arg(x).arg(y);
        default:
            return QString("https://tile.openstreetmap.org/%1/%2/%3.png").arg(zoom).arg(x).arg(y);
    }
}

QString OnlineTileWidget::getFallbackUrl(int zoom, int x, int y) const {
    // Fast German OpenStreetMap mirror as automatic fallback
    return QString("https://tile.openstreetmap.de/%1/%2/%3.png").arg(zoom).arg(x).arg(y);
}

// --- Tile fetching & caching ---

QPixmap* OnlineTileWidget::getTile(int zoom, int x, int y) {
    int maxTile = (1 << zoom);
    x = ((x % maxTile) + maxTile) % maxTile;
    if (y < 0 || y >= maxTile) return nullptr;

    TileKey key{static_cast<int>(currentProvider), zoom, x, y};
    QPixmap* cached = tileCache.object(key);
    if (cached) return cached;

    // Not in cache, fetch it
    fetchTile(zoom, x, y, false);
    return nullptr;
}

void OnlineTileWidget::fetchTile(int zoom, int x, int y, bool isFallback) {
    int maxTile = (1 << zoom);
    x = ((x % maxTile) + maxTile) % maxTile;
    if (y < 0 || y >= maxTile) return;

    TileKey key{static_cast<int>(currentProvider), zoom, x, y};
    if (pendingTiles.contains(key)) return;

    pendingTiles.insert(key);

    QString urlStr = isFallback ? getFallbackUrl(zoom, x, y) : getPrimaryUrl(currentProvider, zoom, x, y);
    QUrl url(urlStr);
    QNetworkRequest request(url);

    // Set valid policy-compliant User-Agent header
    request.setRawHeader("User-Agent", "AssamMapExplorer/1.0 (https://sih.gov.in; team@sih-assam.org)");
    request.setRawHeader("Accept", "image/png,image/jpeg,image/*;q=0.9,*/*;q=0.8");

    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, zoom, x, y, isFallback]() {
        reply->deleteLater();
        pendingTiles.remove(key);

        if (reply->error() != QNetworkReply::NoError) {
            // If primary OSM request failed, try fallback mirror
            if (!isFallback) {
                fetchTile(zoom, x, y, true);
            }
            return;
        }

        QByteArray data = reply->readAll();
        // Check if OSM returned a block tile image
        QByteArray blockedHeader = reply->rawHeader("x-blocked");
        if (!blockedHeader.isEmpty()) {
            if (!isFallback) {
                fetchTile(zoom, x, y, true);
                return;
            }
        }

        QPixmap* pixmap = new QPixmap();
        if (pixmap->loadFromData(data) && !pixmap->isNull()) {
            tileCache.insert(key, pixmap);
            update();
        } else {
            delete pixmap;
            if (!isFallback) {
                fetchTile(zoom, x, y, true);
            }
        }
    });
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

    // Dark background for modern theme
    painter.fillRect(rect(), QColor(24, 24, 27));

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
                painter.fillRect(tileRect, QColor(32, 33, 36));
                painter.setPen(QPen(QColor(45, 48, 52), 1));
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

        // 1. Draw solid lines connecting all placed pins
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
    if (floodSimulation.isActive) {
        const auto* slice = MapCore::DamFloodSimulator::getTimeSlice(floodSimulation, floodSimulation.currentMinute);
        if (slice) {
            auto toCanvasPoint = [&](double lat, double lon) -> QPointF {
                double tX = lonToTileX(lon, zoomLevel);
                double tY = latToTileY(lat, zoomLevel);
                return QPointF(cx + (tX - centerTileX) * TILE_SIZE, cy + (tY - centerTileY) * TILE_SIZE);
            };

            // 1. Full Downstream River Reach Bed & Area under Distance Curve (10% Opacity)
            if (floodSimulation.rawNodes.size() >= 2) {
                std::vector<QPointF> reachPts;
                reachPts.reserve(floodSimulation.rawNodes.size());
                for (const auto& node : floodSimulation.rawNodes) {
                    reachPts.push_back(toCanvasPoint(node.lat, node.lon));
                }

                // Area under full downstream distance curve (10% Opacity)
                QPolygonF reachEnvelope;
                float reachWidth = 14.0f;
                for (size_t i = 0; i < reachPts.size(); ++i) {
                    QPointF dir;
                    if (i == 0) dir = reachPts[1] - reachPts[0];
                    else if (i + 1 == reachPts.size()) dir = reachPts[i] - reachPts[i - 1];
                    else dir = reachPts[i + 1] - reachPts[i - 1];

                    float len = std::hypot(dir.x(), dir.y());
                    if (len > 0.001f) {
                        QPointF norm(-dir.y() / len, dir.x() / len);
                        reachEnvelope.append(reachPts[i] + norm * (reachWidth / 2.0f));
                    }
                }
                for (int i = static_cast<int>(reachPts.size()) - 1; i >= 0; --i) {
                    QPointF dir;
                    if (i == 0) dir = reachPts[1] - reachPts[0];
                    else if (i + 1 == static_cast<int>(reachPts.size())) dir = reachPts[i] - reachPts[i - 1];
                    else dir = reachPts[i + 1] - reachPts[i - 1];

                    float len = std::hypot(dir.x(), dir.y());
                    if (len > 0.001f) {
                        QPointF norm(-dir.y() / len, dir.x() / len);
                        reachEnvelope.append(reachPts[i] - norm * (reachWidth / 2.0f));
                    }
                }

                if (reachEnvelope.size() >= 3) {
                    painter.setBrush(QColor(138, 180, 248, 26)); // 10% opacity area under distance curve
                    painter.setPen(QPen(QColor(138, 180, 248, 45), 1.0, Qt::DotLine));
                    painter.drawPolygon(reachEnvelope);
                }

                // Distance trajectory center path
                QPainterPath fullReachPath;
                fullReachPath.moveTo(reachPts[0]);
                for (size_t i = 1; i < reachPts.size(); ++i) {
                    fullReachPath.lineTo(reachPts[i]);
                }
                painter.setPen(QPen(QColor(138, 180, 248, 65), 1.2, Qt::DotLine, Qt::RoundCap));
                painter.drawPath(fullReachPath);
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
                    if (zoomLevel >= 8) {
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
                        // Overtopping cascade crest arc
                        double spillPulse = std::fmod((flowAnimPhase * 1.2), 16.0);
                        painter.setPen(QPen(QColor(0, 229, 255, 240), 2.2, Qt::SolidLine, Qt::RoundCap));
                        painter.setBrush(Qt::NoBrush);
                        painter.drawEllipse(spillScreen, 6.0 + spillPulse, 6.0 + spillPulse);

                        painter.setPen(QPen(QColor(253, 214, 99, 230), 1.5));
                        painter.setBrush(QColor(253, 214, 99));
                        painter.drawEllipse(spillScreen, 3.0, 3.0);

                        if (zoomLevel >= 8) {
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

                // A. Continuous Flood Inundation Area under the displacement line (10% opacity)
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

                // B. Animated Flowing River Streamline & Moving Particles
                QPainterPath activeReachPath;
                activeReachPath.moveTo(activePts[0]);
                for (size_t i = 1; i < activePts.size(); ++i) {
                    activeReachPath.lineTo(activePts[i]);
                }

                painter.setPen(QPen(QColor(0, 188, 212, 220), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.drawPath(activeReachPath);

                // Animated Flowing River Streamline Dashes (Flowing downstream)
                QPen flowPen(QColor(255, 255, 255, 220), 1.8, Qt::CustomDashLine, Qt::RoundCap);
                QList<qreal> dashes;
                dashes << 6.0 << 6.0;
                flowPen.setDashPattern(dashes);
                flowPen.setDashOffset(flowAnimPhase * 1.5);
                painter.setPen(flowPen);
                painter.drawPath(activeReachPath);

                // Flowing Fluid Particle Pulses
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

                // Defer Milestone Distance Markers to Top Z-Index Layer
                for (size_t i = 0; i < activePts.size(); ++i) {
                    if (i > 0 && i % 8 == 0 && i < floodSimulation.rawNodes.size()) {
                        QPointF cPt = activePts[i];
                        double dist = floodSimulation.rawNodes[i].distanceKm;
                        painter.setPen(QPen(QColor(253, 214, 99, 220), 1.0));
                        painter.setBrush(QColor(253, 214, 99, 230));
                        painter.drawEllipse(cPt, 2.8, 2.8);

                        QString dLabel = QString("%1 km").arg(qRound(dist));
                        topTooltips.push_back([cPt, dLabel](QPainter& p) {
                            QFont mFont("Segoe UI", 7, QFont::DemiBold);
                            p.setFont(mFont);
                            QFontMetricsF mfm(mFont);
                            QRectF mRect(cPt.x() + 6, cPt.y() - mfm.height() / 2.0 - 1, mfm.horizontalAdvance(dLabel) + 8, mfm.height() + 2);
                            p.setBrush(QColor(24, 24, 27, 255)); // 100% opacity solid gray
                            p.setPen(QPen(QColor(63, 63, 70, 255), 1.0));
                            p.drawRoundedRect(mRect, 4.0, 4.0);
                            p.setPen(QColor(228, 228, 231));
                            p.drawText(mRect, Qt::AlignCenter, dLabel);
                        });
                    }
                }
            }

            // 5. Leading Wave Front Distance Marker & Floating Tooltip (Top Z-Index Layer)
            if (slice->frontDistanceKm > 0.05) {
                QPointF frontScreen = toCanvasPoint(slice->leadingFrontPos.x(), slice->leadingFrontPos.y());

                // Pulsing target rings
                double frontPulse = std::fmod((flowAnimPhase * 0.6), 14.0);
                painter.setPen(QPen(QColor(0, 229, 255, 240), 2.0));
                painter.setBrush(QColor(0, 229, 255, 40));
                painter.drawEllipse(frontScreen, 8.0 + frontPulse, 8.0 + frontPulse);

                painter.setPen(QPen(QColor(253, 214, 99), 1.5));
                painter.setBrush(QColor(253, 214, 99));
                painter.drawEllipse(frontScreen, 3.5, 3.5);

                // Defer shadcn Tooltip Badge to Top Z-Index Layer
                QString frontBadge = QString("%1 km · T + %2m")
                    .arg(slice->frontDistanceKm, 0, 'f', 1)
                    .arg(floodSimulation.currentMinute);

                topTooltips.push_back([frontScreen, frontBadge](QPainter& p) {
                    QFont badgeFont("Segoe UI", 8, QFont::DemiBold);
                    p.setFont(badgeFont);
                    QFontMetricsF bfm(badgeFont);
                    double bw = bfm.horizontalAdvance(frontBadge) + 14.0;
                    double bh = bfm.height() + 6.0;
                    QRectF bRect(frontScreen.x() - bw / 2.0, frontScreen.y() - bh - 10.0, bw, bh);

                    p.setBrush(QColor(24, 24, 27, 255)); // 100% opacity solid gray
                    p.setPen(QPen(QColor(63, 63, 70, 255), 1.0));
                    p.drawRoundedRect(bRect, 6.0, 6.0);

                    p.setPen(QColor(244, 244, 245)); // #F4F4F5 zinc-100
                    p.drawText(bRect, Qt::AlignCenter, frontBadge);

                    // Downward pointer arrow (100% opacity)
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
    }

    // --- Render All Tooltips & Badges at Highest Z-Index (Above all paths, dots, and polygons) ---
    for (const auto& renderTooltip : topTooltips) {
        renderTooltip(painter);
    }

    // End Rotated Map Scene
    painter.restore();

    // 2. Draw Upright Overlays & Axis Crosshair (Screen Middle Plus)
    // Center crosshair (the axis of rotation)
    painter.setPen(QPen(QColor(138, 180, 248, 180), 1.5));
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

void OnlineTileWidget::updateFloodSimulationMinute(int minute) {
    if (floodSimulation.isActive) {
        floodSimulation.currentMinute = std::clamp(minute, 0, 60);
        update();
    }
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

    if (isDragging && (event->buttons() & Qt::LeftButton)) {
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

    if (measureMode) {
        update();
    }
}

void OnlineTileWidget::mouseReleaseEvent(QMouseEvent* event) {
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
