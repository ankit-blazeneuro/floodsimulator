#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPixmap>
#include <QCache>
#include <QSet>
#include <QUrl>
#include <QString>
#include <cmath>

namespace MapCore {

enum class OnlineTileProvider {
    OpenStreetMap_Standard = 0,
    OpenStreetMap_DE = 1,
    OpenStreetMap_Voyager = 2,
    OpenStreetMap_Dark = 3
};

struct TileKey {
    int provider;
    int zoom;
    int x;
    int y;

    bool operator==(const TileKey& o) const {
        return provider == o.provider && zoom == o.zoom && x == o.x && y == o.y;
    }
};

inline uint qHash(const TileKey& key, uint seed = 0) {
    return ::qHash(key.provider, seed) ^ ::qHash(key.zoom, seed) ^ ::qHash(key.x, seed) ^ ::qHash(key.y, seed);
}

class TileCacheManager : public QObject {
    Q_OBJECT

public:
    static TileCacheManager& instance();

    QPixmap* getTile(OnlineTileProvider provider, int zoom, int x, int y);
    bool hasTile(OnlineTileProvider provider, int zoom, int x, int y) const;
    void prefetchTile(OnlineTileProvider provider, int zoom, int x, int y);

    void clearCache();
    int cacheCount() const { return m_tileCache.count(); }
    int cacheMaxCost() const { return m_tileCache.maxCost(); }
    void setCacheMaxCost(int maxCost) { m_tileCache.setMaxCost(maxCost); }

    int totalRequestsSent() const { return m_requestsSent; }
    int totalCacheHits() const { return m_cacheHits; }
    int pendingRequestsCount() const { return m_pendingTiles.size(); }

    static double lonToTileX(double lon, int zoom) {
        return (lon + 180.0) / 360.0 * std::pow(2.0, zoom);
    }

    static double latToTileY(double lat, int zoom) {
        double latRad = lat * M_PI / 180.0;
        return (1.0 - std::log(std::tan(latRad) + 1.0 / std::cos(latRad)) / M_PI) / 2.0 * std::pow(2.0, zoom);
    }

    static double tileXToLon(double x, int zoom) {
        return x / std::pow(2.0, zoom) * 360.0 - 180.0;
    }

    static double tileYToLat(double y, int zoom) {
        double n = M_PI - 2.0 * M_PI * y / std::pow(2.0, zoom);
        return 180.0 / M_PI * std::atan(0.5 * (std::exp(n) - std::exp(-n)));
    }

signals:
    void tileLoaded(int provider, int zoom, int x, int y);
    void networkStatsChanged(int requestsSent, int cacheHits);

private:
    explicit TileCacheManager(QObject* parent = nullptr);
    ~TileCacheManager() override = default;

    TileCacheManager(const TileCacheManager&) = delete;
    TileCacheManager& operator=(const TileCacheManager&) = delete;

    QString getPrimaryUrl(OnlineTileProvider provider, int zoom, int x, int y) const;
    QString getFallbackUrl(OnlineTileProvider provider, int zoom, int x, int y) const;
    void fetchTile(OnlineTileProvider provider, int zoom, int x, int y, bool isFallback = false);

    QNetworkAccessManager* m_networkManager;
    QCache<TileKey, QPixmap> m_tileCache;
    QSet<TileKey> m_pendingTiles;

    int m_requestsSent = 0;
    int m_cacheHits = 0;
};

} // namespace MapCore
