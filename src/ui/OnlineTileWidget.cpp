#include "OnlineTileWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace MapUI {

OnlineTileWidget::OnlineTileWidget(QWidget* parent) : QWidget(parent), tileCache(500) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    networkManager = new QNetworkAccessManager(this);

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

    // Dark background for modern theme
    painter.fillRect(rect(), QColor(24, 24, 27));

    // Calculate center tile position
    double centerTileX = lonToTileX(centerLon, zoomLevel);
    double centerTileY = latToTileY(centerLat, zoomLevel);

    // Pixel offset from center of the widget to the top-left corner of the center tile
    double offsetX = w / 2.0 - (centerTileX - std::floor(centerTileX)) * TILE_SIZE;
    double offsetY = h / 2.0 - (centerTileY - std::floor(centerTileY)) * TILE_SIZE;

    int centerTileXInt = static_cast<int>(std::floor(centerTileX));
    int centerTileYInt = static_cast<int>(std::floor(centerTileY));

    // How many tiles to cover viewport
    int tilesLeft = static_cast<int>(std::ceil(offsetX / TILE_SIZE)) + 1;
    int tilesRight = static_cast<int>(std::ceil((w - offsetX) / static_cast<double>(TILE_SIZE))) + 1;
    int tilesTop = static_cast<int>(std::ceil(offsetY / TILE_SIZE)) + 1;
    int tilesBottom = static_cast<int>(std::ceil((h - offsetY) / static_cast<double>(TILE_SIZE))) + 1;

    for (int dy = -tilesTop; dy <= tilesBottom; ++dy) {
        for (int dx = -tilesLeft; dx <= tilesRight; ++dx) {
            int tileX = centerTileXInt + dx;
            int tileY = centerTileYInt + dy;

            int drawX = static_cast<int>(offsetX + dx * TILE_SIZE);
            int drawY = static_cast<int>(offsetY + dy * TILE_SIZE);

            QPixmap* tile = getTile(zoomLevel, tileX, tileY);
            if (tile && !tile->isNull()) {
                painter.drawPixmap(drawX, drawY, TILE_SIZE, TILE_SIZE, *tile);
            } else {
                // Placeholder tile
                QRect tileRect(drawX, drawY, TILE_SIZE, TILE_SIZE);
                painter.fillRect(tileRect, QColor(32, 33, 36));
                painter.setPen(QPen(QColor(45, 48, 52), 1));
                painter.drawRect(tileRect);

                painter.setPen(QColor(113, 113, 122));
                QFont loadFont("Segoe UI", 8);
                painter.setFont(loadFont);
                painter.drawText(tileRect, Qt::AlignCenter, "Loading...");
            }
        }
    }

    // Draw center crosshair
    painter.setPen(QPen(QColor(138, 180, 248, 140), 1.5));
    painter.drawLine(w / 2 - 8, h / 2, w / 2 + 8, h / 2);
    painter.drawLine(w / 2, h / 2 - 8, w / 2, h / 2 + 8);

    // Required Attribution text
    painter.setPen(QColor(230, 230, 230));
    QFont attrFont("Segoe UI", 9, QFont::Medium);
    painter.setFont(attrFont);
    QString attribution = "© OpenStreetMap contributors";
    QRect attrRect(w - 240, h - 26, 230, 20);
    painter.fillRect(attrRect.adjusted(-6, -2, 6, 2), QColor(20, 20, 22, 220));
    painter.drawText(attrRect, Qt::AlignRight | Qt::AlignVCenter, attribution);
}

// --- Mouse interaction ---

void OnlineTileWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        lastMousePos = event->pos();
        setCursor(Qt::ClosedHandCursor);
    }
}

void OnlineTileWidget::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging) {
        QPoint delta = event->pos() - lastMousePos;
        lastMousePos = event->pos();

        // Convert pixel delta to geographic coordinate delta
        double tileCount = std::pow(2.0, zoomLevel);
        double lonDelta = -delta.x() / (TILE_SIZE * tileCount) * 360.0;

        double centerTileY = latToTileY(centerLat, zoomLevel);
        double newTileY = centerTileY - delta.y() / static_cast<double>(TILE_SIZE);
        double newLat = tileYToLat(newTileY, zoomLevel);

        centerLon = std::clamp(centerLon + lonDelta, -180.0, 180.0);
        centerLat = std::clamp(newLat, -85.0511, 85.0511);

        emitViewportChanged();
        update();
    }
}

void OnlineTileWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void OnlineTileWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
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
