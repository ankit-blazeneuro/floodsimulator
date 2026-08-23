#include "DamManager.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDataStream>
#include <iostream>
#include <cmath>

namespace MapCore {

double DamManager::parseDmsCoordinate(const QString& dms) {
    if (dms.isEmpty()) return 0.0;

    static QRegularExpression re(R"re((\d+)°\s*(\d+)'\s*([0-9.]+)"?\s*([NSEWnsew]))re");
    QRegularExpressionMatch match = re.match(dms.trimmed());
    if (match.hasMatch()) {
        double deg = match.captured(1).toDouble();
        double min = match.captured(2).toDouble();
        double sec = match.captured(3).toDouble();
        QString dir = match.captured(4).toUpper();

        double val = deg + (min / 60.0) + (sec / 3600.0);
        if (dir == "S" || dir == "W") {
            val = -val;
        }
        return val;
    }

    bool ok = false;
    double directVal = dms.toDouble(&ok);
    return ok ? directVal : 0.0;
}

void DamManager::buildSpatialIndex() {
    for (int r = 0; r < GRID_ROWS; ++r) {
        for (int c = 0; c < GRID_COLS; ++c) {
            spatialGrid[r][c].clear();
        }
    }

    for (size_t i = 0; i < dams.size(); ++i) {
        const auto& d = dams[i];
        int row = static_cast<int>((d.lat - GRID_MIN_LAT) / CELL_SIZE);
        int col = static_cast<int>((d.lon - GRID_MIN_LON) / CELL_SIZE);

        if (row >= 0 && row < GRID_ROWS && col >= 0 && col < GRID_COLS) {
            spatialGrid[row][col].push_back(static_cast<uint32_t>(i));
        }
    }
}

bool DamManager::loadFromGeoJson(const QString& filePath) {
    // Try binary cache first for ultra-fast startup
    QStringList cacheCandidates = {
        "data/dams_cache.bin",
        "../data/dams_cache.bin"
    };
    for (const auto& cp : cacheCandidates) {
        if (QFileInfo::exists(cp)) {
            if (loadFromBinary(cp)) {
                std::cout << "[DamManager] Loaded " << dams.size() << " dams from binary cache." << std::endl;
                return true;
            }
        }
    }

    // Find GeoJSON from candidate paths
    QStringList geoCandidates = {
        filePath,
        "server/dam.geojson",
        "../server/dam.geojson",
        "data/dam.geojson",
        "../data/dam.geojson"
    };

    QString foundPath;
    for (const auto& gp : geoCandidates) {
        if (QFileInfo::exists(gp)) {
            foundPath = gp;
            break;
        }
    }

    if (foundPath.isEmpty()) {
        std::cerr << "[DamManager] Could not find dam.geojson in candidate paths." << std::endl;
        return false;
    }

    QFile file(foundPath);
    if (!file.open(QIODevice::ReadOnly)) {
        std::cerr << "[DamManager] Failed to open: " << foundPath.toStdString() << std::endl;
        return false;
    }

    QByteArray rawData = file.readAll();
    file.close();

    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(rawData, &parseErr);
    if (parseErr.error != QJsonParseError::NoError) {
        std::cerr << "[DamManager] JSON Parse Error: " << parseErr.errorString().toStdString() << std::endl;
        return false;
    }

    QJsonObject rootObj = doc.object();
    QJsonArray features = rootObj["features"].toArray();

    dams.clear();
    dams.reserve(features.size());

    for (const QJsonValue& fVal : features) {
        QJsonObject feat = fVal.toObject();
        QJsonObject props = feat["properties"].toObject();

        DamPoint dam;
        dam.pic = props["PIC"].toString();
        dam.name = props["dm_name"].toString().trimmed();
        if (dam.name.isEmpty()) dam.name = "Unnamed Dam";

        dam.state = props["state"].toString();
        dam.district = props["district"].toString();
        dam.river = props["river"].toString();
        dam.basin = props["basin"].toString();
        dam.purpose = props["purpose"].toString();
        dam.damType = props["dm_type"].toString();
        dam.incharge = props["incharge"].toString();

        dam.height = static_cast<float>(props["ht_found"].toDouble(0.0));
        dam.storage = static_cast<float>(props["gs_st_cap"].toDouble(0.0));
        dam.spillwayCap = static_cast<float>(props["ds_sp_cap"].toDouble(0.0));
        dam.year = props["cmp_year"].toInt(0);

        QString latStr = props["latitude"].toString();
        QString lonStr = props["longitude"].toString();

        dam.lat = parseDmsCoordinate(latStr);
        dam.lon = parseDmsCoordinate(lonStr);

        // Verify bounds in India region (lat: 5.0 to 39.0, lon: 65.0 to 100.0)
        if (dam.lat >= 5.0 && dam.lat <= 39.0 && dam.lon >= 65.0 && dam.lon <= 100.0) {
            dams.push_back(dam);
        }
    }

    isLoaded = true;
    buildSpatialIndex();
    std::cout << "[DamManager] Parsed & indexed " << dams.size() << " dams from " << foundPath.toStdString() << std::endl;

    // Save binary cache for subsequent instant loads
    saveToBinary("data/dams_cache.bin");
    saveToBinary("../data/dams_cache.bin");
    return true;
}

bool DamManager::loadFromBinary(const QString& cachePath) {
    QFile file(cachePath);
    if (!file.open(QIODevice::ReadOnly)) return false;

    QDataStream in(&file);
    in.setVersion(QDataStream::Qt_6_0);

    quint32 magic = 0;
    in >> magic;
    if (magic != 0x44414D53) { // "DAMS"
        return false;
    }

    quint32 count = 0;
    in >> count;

    dams.clear();
    dams.reserve(count);

    for (quint32 i = 0; i < count; ++i) {
        DamPoint d;
        in >> d.pic >> d.name >> d.state >> d.district >> d.river >> d.basin
           >> d.lat >> d.lon >> d.height >> d.storage >> d.spillwayCap >> d.year
           >> d.purpose >> d.damType >> d.incharge;
        dams.push_back(d);
    }

    isLoaded = true;
    buildSpatialIndex();
    return true;
}

bool DamManager::saveToBinary(const QString& cachePath) const {
    QFileInfo fi(cachePath);
    QDir().mkpath(fi.dir().absolutePath());

    QFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly)) return false;

    QDataStream out(&file);
    out.setVersion(QDataStream::Qt_6_0);

    out << quint32(0x44414D53); // Magic "DAMS"
    out << quint32(dams.size());

    for (const auto& d : dams) {
        out << d.pic << d.name << d.state << d.district << d.river << d.basin
            << d.lat << d.lon << d.height << d.storage << d.spillwayCap << d.year
            << d.purpose << d.damType << d.incharge;
    }

    return true;
}

void DamManager::getDamsInBbox(double minLat, double minLon, double maxLat, double maxLon, std::vector<const DamPoint*>& outDams) const {
    outDams.clear();
    if (!isLoaded || dams.empty()) return;

    int minRow = std::clamp(static_cast<int>((minLat - GRID_MIN_LAT) / CELL_SIZE), 0, GRID_ROWS - 1);
    int maxRow = std::clamp(static_cast<int>((maxLat - GRID_MIN_LAT) / CELL_SIZE), 0, GRID_ROWS - 1);
    int minCol = std::clamp(static_cast<int>((minLon - GRID_MIN_LON) / CELL_SIZE), 0, GRID_COLS - 1);
    int maxCol = std::clamp(static_cast<int>((maxLon - GRID_MIN_LON) / CELL_SIZE), 0, GRID_COLS - 1);

    for (int r = minRow; r <= maxRow; ++r) {
        for (int c = minCol; c <= maxCol; ++c) {
            const auto& cell = spatialGrid[r][c];
            for (uint32_t idx : cell) {
                const DamPoint& d = dams[idx];
                if (d.lat >= minLat && d.lat <= maxLat && d.lon >= minLon && d.lon <= maxLon) {
                    outDams.push_back(&d);
                }
            }
        }
    }
}

const DamPoint* DamManager::findNearest(double lat, double lon, double maxDistDeg) const {
    const DamPoint* best = nullptr;
    double bestDistSq = maxDistDeg * maxDistDeg;

    int minRow = std::clamp(static_cast<int>((lat - maxDistDeg - GRID_MIN_LAT) / CELL_SIZE), 0, GRID_ROWS - 1);
    int maxRow = std::clamp(static_cast<int>((lat + maxDistDeg - GRID_MIN_LAT) / CELL_SIZE), 0, GRID_ROWS - 1);
    int minCol = std::clamp(static_cast<int>((lon - maxDistDeg - GRID_MIN_LON) / CELL_SIZE), 0, GRID_COLS - 1);
    int maxCol = std::clamp(static_cast<int>((lon + maxDistDeg - GRID_MIN_LON) / CELL_SIZE), 0, GRID_COLS - 1);

    for (int r = minRow; r <= maxRow; ++r) {
        for (int c = minCol; c <= maxCol; ++c) {
            const auto& cell = spatialGrid[r][c];
            for (uint32_t idx : cell) {
                const DamPoint& dam = dams[idx];
                double dLat = dam.lat - lat;
                double dLon = dam.lon - lon;
                double distSq = dLat * dLat + dLon * dLon;
                if (distSq < bestDistSq) {
                    bestDistSq = distSq;
                    best = &dam;
                }
            }
        }
    }
    return best;
}

} // namespace MapCore
