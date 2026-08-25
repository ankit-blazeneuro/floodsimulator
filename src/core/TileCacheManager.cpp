#include "TileCacheManager.h"
#include <QDebug>
#include <algorithm>

namespace MapCore {

TileCacheManager& TileCacheManager::instance() {
    static TileCacheManager s_instance;
    return s_instance;
}

TileCacheManager::TileCacheManager(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_tileCache(3000) {
}

QString TileCacheManager::getPrimaryUrl(OnlineTileProvider provider, int zoom, int x, int y) const {
    static const char subdomains[] = {'a', 'b', 'c', 'd'};
    char sub = subdomains[std::abs(x + y) % 4];
    switch (provider) {
        case OnlineTileProvider::OpenStreetMap_Standard:
            return QString("https://%1.tile.openstreetmap.org/%2/%3/%4.png").arg(sub).arg(zoom).arg(x).arg(y);
        case OnlineTileProvider::OpenStreetMap_DE:
            return QString("https://tile.openstreetmap.de/%1/%2/%3.png").arg(zoom).arg(x).arg(y);
        case OnlineTileProvider::OpenStreetMap_Voyager:
            return QString("https://%1.basemaps.cartocdn.com/rastertiles/voyager/%2/%3/%4.png").arg(sub).arg(zoom).arg(x).arg(y);
        case OnlineTileProvider::OpenStreetMap_Dark:
            return QString("https://%1.basemaps.cartocdn.com/dark_all/%2/%3/%4.png").arg(sub).arg(zoom).arg(x).arg(y);
        default:
            return QString("https://%1.tile.openstreetmap.org/%2/%3/%4.png").arg(sub).arg(zoom).arg(x).arg(y);
    }
}

QString TileCacheManager::getFallbackUrl(OnlineTileProvider provider, int zoom, int x, int y) const {
    static const char subdomains[] = {'c', 'd', 'a', 'b'};
    char sub = subdomains[std::abs(x + y) % 4];
    switch (provider) {
        case OnlineTileProvider::OpenStreetMap_Dark:
            return QString("https://%1.basemaps.cartocdn.com/dark_all/%2/%3/%4.png").arg(sub).arg(zoom).arg(x).arg(y);
        case OnlineTileProvider::OpenStreetMap_Voyager:
            return QString("https://%1.basemaps.cartocdn.com/rastertiles/voyager/%2/%3/%4.png").arg(sub).arg(zoom).arg(x).arg(y);
        case OnlineTileProvider::OpenStreetMap_DE:
            return QString("https://%1.tile.openstreetmap.org/%2/%3/%4.png").arg(sub).arg(zoom).arg(x).arg(y);
        case OnlineTileProvider::OpenStreetMap_Standard:
        default:
            return QString("https://tile.openstreetmap.de/%1/%2/%3.png").arg(zoom).arg(x).arg(y);
    }
}

QPixmap* TileCacheManager::getTile(OnlineTileProvider provider, int zoom, int x, int y) {
    int maxTile = (1 << zoom);
    x = ((x % maxTile) + maxTile) % maxTile;
    if (y < 0 || y >= maxTile) return nullptr;

    TileKey key{static_cast<int>(provider), zoom, x, y};
    QPixmap* cached = m_tileCache.object(key);
    if (cached) {
        ++m_cacheHits;
        return cached;
    }

    // Not in memory cache -> initiate single fetch if not already pending
    fetchTile(provider, zoom, x, y, false);
    return nullptr;
}

bool TileCacheManager::hasTile(OnlineTileProvider provider, int zoom, int x, int y) const {
    int maxTile = (1 << zoom);
    x = ((x % maxTile) + maxTile) % maxTile;
    if (y < 0 || y >= maxTile) return false;

    TileKey key{static_cast<int>(provider), zoom, x, y};
    return m_tileCache.contains(key);
}

void TileCacheManager::prefetchTile(OnlineTileProvider provider, int zoom, int x, int y) {
    int maxTile = (1 << zoom);
    x = ((x % maxTile) + maxTile) % maxTile;
    if (y < 0 || y >= maxTile) return;

    TileKey key{static_cast<int>(provider), zoom, x, y};
    if (m_tileCache.contains(key) || m_pendingTiles.contains(key)) return;

    fetchTile(provider, zoom, x, y, false);
}

void TileCacheManager::clearCache() {
    m_tileCache.clear();
}

void TileCacheManager::fetchTile(OnlineTileProvider provider, int zoom, int x, int y, bool isFallback) {
    int maxTile = (1 << zoom);
    x = ((x % maxTile) + maxTile) % maxTile;
    if (y < 0 || y >= maxTile) return;

    TileKey key{static_cast<int>(provider), zoom, x, y};
    if (m_pendingTiles.contains(key)) {
        // Already being downloaded over network by another grid cell/map widget
        return;
    }

    m_pendingTiles.insert(key);
    ++m_requestsSent;
    emit networkStatsChanged(m_requestsSent, m_cacheHits);

    QString urlStr = isFallback ? getFallbackUrl(provider, zoom, x, y) : getPrimaryUrl(provider, zoom, x, y);
    QUrl url(urlStr);
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "AssamMapExplorer/1.0 (https://sih.gov.in; team@sih-assam.org)");
    request.setRawHeader("Accept", "image/png,image/jpeg,image/*;q=0.9,*/*;q=0.8");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, provider, zoom, x, y, isFallback]() {
        reply->deleteLater();
        m_pendingTiles.remove(key);

        if (reply->error() != QNetworkReply::NoError) {
            if (!isFallback) {
                fetchTile(provider, zoom, x, y, true);
            }
            return;
        }

        QByteArray data = reply->readAll();
        QByteArray blockedHeader = reply->rawHeader("x-blocked");
        if (!blockedHeader.isEmpty() && !isFallback) {
            fetchTile(provider, zoom, x, y, true);
            return;
        }

        auto* pixmap = new QPixmap();
        if (pixmap->loadFromData(data) && !pixmap->isNull()) {
            m_tileCache.insert(key, pixmap);
            emit tileLoaded(static_cast<int>(provider), zoom, x, y);
        } else {
            delete pixmap;
            if (!isFallback) {
                fetchTile(provider, zoom, x, y, true);
            }
        }
    });
}

} // namespace MapCore
