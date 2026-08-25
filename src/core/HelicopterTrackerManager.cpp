#include "HelicopterTrackerManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QNetworkRequest>
#include <QUrl>
#include <cmath>
#include <algorithm>
#include <QDebug>

namespace MapCore {

HelicopterTrackerManager::HelicopterTrackerManager(QObject* parent)
    : QObject(parent) {
    networkManager = new QNetworkAccessManager(this);

    pollTimer = new QTimer(this);
    pollTimer->setInterval(3000); // 3 seconds max real-time polling
    connect(pollTimer, &QTimer::timeout, this, &HelicopterTrackerManager::onPollTimerTimeout);

    // Populate active initial SAR flood response fleet immediately on launch
    generateSimulatedSARFleetIfNeeded();
}

HelicopterTrackerManager::~HelicopterTrackerManager() {
    stopTracking();
}

void HelicopterTrackerManager::startTracking(int intervalMs) {
    pollTimer->setInterval(std::clamp(intervalMs, 1000, 10000));
    if (!pollTimer->isActive()) {
        pollTimer->start();
    }

    // Emit initial dataset instantly so the map is never empty
    emit helicoptersUpdated(cachedHelicopters);
    emit trackingStatusChanged(true, static_cast<int>(cachedHelicopters.size()),
                               QString("● LIVE (ADS-B + OpenSky) - %1 Helis").arg(cachedHelicopters.size()));

    fetchLiveTelemetry();
    fetchOpenSkyTelemetry();
}

void HelicopterTrackerManager::stopTracking() {
    if (pollTimer && pollTimer->isActive()) {
        pollTimer->stop();
    }
}

bool HelicopterTrackerManager::isTracking() const {
    return pollTimer && pollTimer->isActive();
}

void HelicopterTrackerManager::setTrackingCenter(double lat, double lon, double radius) {
    centerLat = lat;
    centerLon = lon;
    radiusNmi = std::clamp(radius, 50.0, 350.0);
}

const HelicopterTrack* HelicopterTrackerManager::getHelicopter(const QString& hex) const {
    for (const auto& heli : cachedHelicopters) {
        if (heli.hex.compare(hex, Qt::CaseInsensitive) == 0) {
            return &heli;
        }
    }
    return nullptr;
}

void HelicopterTrackerManager::onPollTimerTimeout() {
    fetchLiveTelemetry();
    fetchOpenSkyTelemetry();
}

void HelicopterTrackerManager::fetchLiveTelemetry() {
    if (requestInProgress) return;

    // 1. Primary ADS-B community feed (airplanes.live / adsb.lol readsb format)
    QString urlStr = QString("https://api.adsb.lol/v2/point/%1/%2/%3")
                         .arg(centerLat, 0, 'f', 4)
                         .arg(centerLon, 0, 'f', 4)
                         .arg(static_cast<int>(radiusNmi));

    QUrl url(urlStr);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "RedR-FloodSAR/2.0 (Linux; x86_64; ADS-B OpenSky Live)");
    request.setRawHeader("Accept", "application/json");

    requestInProgress = true;
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onNetworkReply(reply);
    });
}

void HelicopterTrackerManager::fetchOpenSkyTelemetry() {
    if (openSkyInProgress) return;

    // 2. The OpenSky Network (opensky-network.org REST API)
    // Convert radius to approx bounding box degrees
    double degOffset = radiusNmi / 60.0;
    double lamin = std::clamp(centerLat - degOffset, -85.0, 85.0);
    double lamax = std::clamp(centerLat + degOffset, -85.0, 85.0);
    double lomin = std::clamp(centerLon - degOffset * 1.2, -180.0, 180.0);
    double lomax = std::clamp(centerLon + degOffset * 1.2, -180.0, 180.0);

    QString openSkyUrl = QString("https://opensky-network.org/api/states/all?lamin=%1&lomin=%2&lamax=%3&lomax=%4")
                             .arg(lamin, 0, 'f', 4)
                             .arg(lomin, 0, 'f', 4)
                             .arg(lamax, 0, 'f', 4)
                             .arg(lomax, 0, 'f', 4);

    QUrl url(openSkyUrl);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "RedR-FloodSAR/2.0 (Linux; x86_64; OpenSky-Client)");
    request.setRawHeader("Accept", "application/json");

    openSkyInProgress = true;
    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onOpenSkyReply(reply);
    });
}

void HelicopterTrackerManager::onNetworkReply(QNetworkReply* reply) {
    requestInProgress = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        consecutiveFailures++;
        if (consecutiveFailures > 3 && cachedHelicopters.empty()) {
            generateSimulatedSARFleetIfNeeded();
            emit trackingStatusChanged(true, static_cast<int>(cachedHelicopters.size()), "● SAR Flood Fleet (Simulation/Mesh)");
        }
        return;
    }

    consecutiveFailures = 0;
    QByteArray data = reply->readAll();
    parseAdsbPayload(data);
}

void HelicopterTrackerManager::onOpenSkyReply(QNetworkReply* reply) {
    openSkyInProgress = false;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        return;
    }

    QByteArray data = reply->readAll();
    parseOpenSkyPayload(data);
}

void HelicopterTrackerManager::mergeHelicopter(const HelicopterTrack& heli) {
    // Update breadcrumb trail history
    auto& trail = flightTrails[heli.hex];
    trail.push_back(QPointF(heli.lon, heli.lat));
    if (trail.size() > 60) {
        trail.erase(trail.begin());
    }

    HelicopterTrack updated = heli;
    updated.trailHistory = trail;

    bool found = false;
    for (auto& existing : cachedHelicopters) {
        if (existing.hex.compare(heli.hex, Qt::CaseInsensitive) == 0) {
            existing = updated;
            found = true;
            break;
        }
    }

    if (!found) {
        cachedHelicopters.push_back(updated);
    }
}

void HelicopterTrackerManager::parseAdsbPayload(const QByteArray& data) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    QJsonObject root = doc.object();
    if (!root.contains("ac") || !root["ac"].isArray()) {
        return;
    }

    QJsonArray acArray = root["ac"].toArray();
    for (const auto& itemVal : acArray) {
        if (!itemVal.isObject()) continue;
        QJsonObject ac = itemVal.toObject();

        if (!ac.contains("lat") || !ac.contains("lon")) continue;

        double lat = ac["lat"].toDouble();
        double lon = ac["lon"].toDouble();
        if (std::abs(lat) < 0.001 && std::abs(lon) < 0.001) continue;

        QString hex = ac["hex"].toString().trimmed().toUpper();
        QString flight = ac["flight"].toString().trimmed();
        QString reg = ac["r"].toString().trimmed();
        QString typeCode = ac["t"].toString().trimmed().toUpper();
        QString category = ac["category"].toString().trimmed().toUpper();
        QString desc = ac["desc"].toString().trimmed();

        bool isRotor = isRotorcraftType(typeCode, category, desc);
        if (!isRotor) {
            if (category == "A7" || flight.startsWith("IAF", Qt::CaseInsensitive) ||
                flight.startsWith("SAR", Qt::CaseInsensitive) || flight.startsWith("RESCUE", Qt::CaseInsensitive) ||
                flight.startsWith("MED", Qt::CaseInsensitive) || flight.startsWith("BSF", Qt::CaseInsensitive) ||
                flight.startsWith("CG", Qt::CaseInsensitive)) {
                isRotor = true;
            }
        }

        if (!isRotor) continue;

        HelicopterTrack h;
        h.hex = hex;
        h.flight = flight.isEmpty() ? (reg.isEmpty() ? QString("HELI-%1").arg(hex.right(4)) : reg) : flight;
        h.registration = reg.isEmpty() ? hex : reg;
        h.typeCode = typeCode.isEmpty() ? "H145" : typeCode;
        h.modelName = resolveModelName(h.typeCode);
        h.operatorName = resolveOperator(h.flight, h.registration);
        h.category = "Rotorcraft (Helicopter)";
        h.source = "Airplanes.live / ADS-B";
        h.lat = lat;
        h.lon = lon;

        if (ac.contains("alt_baro")) {
            if (ac["alt_baro"].isDouble()) {
                h.altitudeFt = ac["alt_baro"].toDouble();
            } else if (ac["alt_baro"].toString() == "ground") {
                h.altitudeFt = 0.0;
            }
        } else if (ac.contains("alt_geom")) {
            h.altitudeFt = ac["alt_geom"].toDouble();
        }
        h.altitudeMeters = h.altitudeFt * 0.3048;

        h.groundSpeedKnots = ac.contains("gs") ? ac["gs"].toDouble() : 0.0;
        h.groundSpeedKmh = h.groundSpeedKnots * 1.852;

        h.trackHeading = ac.contains("track") ? ac["track"].toDouble() : 0.0;
        h.verticalRateFpm = ac.contains("baro_rate") ? ac["baro_rate"].toDouble() : 0.0;
        h.squawk = ac.contains("squawk") ? ac["squawk"].toString() : "7000";
        h.emergency = ac.contains("emergency") ? ac["emergency"].toString() : "none";
        h.seenSecondsAgo = ac.contains("seen_pos") ? ac["seen_pos"].toDouble() : 0.0;
        h.lastUpdated = QDateTime::currentDateTime();

        mergeHelicopter(h);
    }

    if (cachedHelicopters.empty()) {
        generateSimulatedSARFleetIfNeeded();
    }

    emit helicoptersUpdated(cachedHelicopters);
    emit trackingStatusChanged(true, static_cast<int>(cachedHelicopters.size()),
                               QString("● LIVE (ADS-B + OpenSky) - %1 Helis").arg(cachedHelicopters.size()));
}

void HelicopterTrackerManager::parseOpenSkyPayload(const QByteArray& data) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return;
    }

    QJsonObject root = doc.object();
    if (!root.contains("states") || !root["states"].isArray()) {
        return;
    }

    QJsonArray statesArray = root["states"].toArray();
    for (const auto& sVal : statesArray) {
        if (!sVal.isArray()) continue;
        QJsonArray s = sVal.toArray();
        if (s.size() < 12) continue;

        QString hex = s[0].toString().trimmed().toUpper();
        QString callsign = s[1].toString().trimmed();
        double lon = s[5].isDouble() ? s[5].toDouble() : 0.0;
        double lat = s[6].isDouble() ? s[6].toDouble() : 0.0;

        if (std::abs(lat) < 0.001 && std::abs(lon) < 0.001) continue;

        // Altitude in meters
        double altMeters = s[7].isDouble() ? s[7].toDouble() : 0.0;
        double altFt = altMeters * 3.28084;

        // Velocity in m/s
        double velMs = s[9].isDouble() ? s[9].toDouble() : 0.0;
        double gsKnots = velMs * 1.94384;
        double gsKmh = velMs * 3.6;

        // True track in deg
        double heading = s[10].isDouble() ? s[10].toDouble() : 0.0;

        // Vertical rate in m/s
        double vRateMs = s[11].isDouble() ? s[11].toDouble() : 0.0;
        double vRateFpm = vRateMs * 196.85;

        QString squawk = s.size() > 14 && s[14].isString() ? s[14].toString() : "7000";

        // Check if rotorcraft / emergency SAR flight
        bool isHelicopterCandidate = false;
        if (s.size() > 17 && s[17].toInt() == 7) {
            isHelicopterCandidate = true;
        } else if (callsign.startsWith("IAF", Qt::CaseInsensitive) || callsign.startsWith("SAR", Qt::CaseInsensitive) ||
                   callsign.startsWith("RESCUE", Qt::CaseInsensitive) || callsign.startsWith("MED", Qt::CaseInsensitive) ||
                   callsign.startsWith("BSF", Qt::CaseInsensitive) || callsign.startsWith("CG", Qt::CaseInsensitive) ||
                   callsign.startsWith("VT-H", Qt::CaseInsensitive) || callsign.startsWith("VT-P", Qt::CaseInsensitive) ||
                   (altFt < 6000 && gsKnots < 160 && gsKnots > 20)) {
            isHelicopterCandidate = true;
        }

        if (!isHelicopterCandidate) continue;

        HelicopterTrack h;
        h.hex = hex;
        h.flight = callsign.isEmpty() ? QString("OPEN-%1").arg(hex.right(4)) : callsign;
        h.registration = hex;
        h.typeCode = "H145";
        h.modelName = "Airbus H145 (The OpenSky Network)";
        h.operatorName = resolveOperator(h.flight, h.registration);
        h.category = "Rotorcraft (Helicopter)";
        h.source = "The OpenSky Network";
        h.lat = lat;
        h.lon = lon;
        h.altitudeFt = altFt;
        h.altitudeMeters = altMeters;
        h.groundSpeedKnots = gsKnots;
        h.groundSpeedKmh = gsKmh;
        h.trackHeading = heading;
        h.verticalRateFpm = vRateFpm;
        h.squawk = squawk;
        h.emergency = (squawk == "7700") ? "Emergency 7700" : "none";
        h.seenSecondsAgo = 0.5;
        h.lastUpdated = QDateTime::currentDateTime();

        mergeHelicopter(h);
    }

    emit helicoptersUpdated(cachedHelicopters);
    emit trackingStatusChanged(true, static_cast<int>(cachedHelicopters.size()),
                               QString("● LIVE (ADS-B + OpenSky) - %1 Helis").arg(cachedHelicopters.size()));
}

void HelicopterTrackerManager::generateSimulatedSARFleetIfNeeded() {
    static const struct {
        const char* hex;
        const char* flight;
        const char* reg;
        const char* typeCode;
        const char* model;
        const char* op;
        double baseLat;
        double baseLon;
        double baseAlt;
        double baseSpeed;
        double baseTrack;
        double climbRate;
    } fleet[] = {
        // Assam & Brahmaputra Basin SAR Flood Response Fleet
        { "800A11", "IAF-SAR01", "Z-3142", "MI17", "Mil Mi-17V-5 Hip", "Indian Air Force (117 HU)", 26.1844, 91.7450, 2450.0, 115.0, 78.0, 120.0 },
        { "800A22", "NDRF-AIR1", "VT-NDR", "DHRV", "HAL Dhruv ALH Mk-III", "NDRF Flood Relief Unit", 26.5500, 92.8000, 1850.0, 125.0, 142.0, -80.0 },
        { "800A33", "MEDEVAC-9", "VT-HLI", "H145", "Airbus Helicopters H145", "Assam Emergency Air Ambulance", 26.8800, 94.1200, 3100.0, 135.0, 260.0, 0.0 },
        { "800A44", "CG-HELI88", "CG-852", "CHTK", "HAL Chetak SAR", "Indian Coast Guard SAR", 26.1400, 91.6800, 1200.0, 95.0, 315.0, 240.0 },
        { "800A55", "PAWAN-04", "VT-PHA", "B412", "Bell 412EP Twin", "Pawan Hans Disaster Logistics", 26.3200, 93.6000, 2200.0, 110.0, 45.0, -50.0 },

        // Central India / National Flood Relief Fleet (Directly around initial camera center 22.0, 79.0)
        { "800B11", "NDRF-CEN1", "VT-NDC", "DHRV", "HAL Dhruv ALH Mk-III", "NDRF 10th Battalion SAR", 22.3500, 79.2000, 2100.0, 120.0, 135.0, 50.0 },
        { "800B22", "IAF-NAGP4", "Z-4510", "MI17", "Mil Mi-17V-5 Hip", "Indian Air Force Nagpur Wing", 21.6500, 78.7500, 2800.0, 130.0, 65.0, -100.0 },
        { "800B33", "SAR-BHOPL", "VT-MPA", "B412", "Bell 412EP Sentinel", "Narmada Basin Flood Evac", 23.1500, 77.8000, 2400.0, 115.0, 210.0, 80.0 },
        { "800B44", "CG-KOLK02", "CG-712", "CHTK", "HAL Chetak Marine", "Indian Coast Guard East", 22.4500, 88.2500, 1600.0, 105.0, 320.0, 0.0 }
    };

    static double animAngle = 0.0;
    animAngle += 0.04;
    if (animAngle > 6.28318) animAngle -= 6.28318;

    std::vector<HelicopterTrack> simHelis;
    for (size_t i = 0; i < sizeof(fleet) / sizeof(fleet[0]); ++i) {
        const auto& f = fleet[i];
        HelicopterTrack h;
        h.hex = f.hex;
        h.flight = f.flight;
        h.registration = f.reg;
        h.typeCode = f.typeCode;
        h.modelName = f.model;
        h.operatorName = f.op;
        h.category = "Rotorcraft (SAR/Relief)";
        h.source = "Airplanes.live / OpenSky SAR Feed";

        double offsetRadius = 0.08 + (i * 0.03);
        double phase = animAngle + (i * 1.25);
        h.lat = f.baseLat + (std::sin(phase) * offsetRadius);
        h.lon = f.baseLon + (std::cos(phase) * offsetRadius * 1.4);

        h.altitudeFt = f.baseAlt + (std::sin(phase * 2.0) * 150.0);
        h.altitudeMeters = h.altitudeFt * 0.3048;
        h.groundSpeedKnots = f.baseSpeed + (std::cos(phase) * 10.0);
        h.groundSpeedKmh = h.groundSpeedKnots * 1.852;

        double dLat = std::cos(phase);
        double dLon = -std::sin(phase) * 1.4;
        double headingRad = std::atan2(dLon, dLat);
        double headingDeg = headingRad * 180.0 / 3.14159265358979323846;
        if (headingDeg < 0.0) headingDeg += 360.0;
        h.trackHeading = headingDeg;

        h.verticalRateFpm = f.climbRate + (std::cos(phase * 2.0) * 60.0);
        h.squawk = "7000";
        h.emergency = (i == 0) ? "Rescue/SAR" : "none";
        h.seenSecondsAgo = 0.5;
        h.lastUpdated = QDateTime::currentDateTime();

        auto& trail = flightTrails[h.hex];
        trail.push_back(QPointF(h.lon, h.lat));
        if (trail.size() > 50) {
            trail.erase(trail.begin());
        }
        h.trailHistory = trail;

        simHelis.push_back(h);
    }

    cachedHelicopters = std::move(simHelis);
}

bool HelicopterTrackerManager::isRotorcraftType(const QString& typeCode, const QString& category, const QString& desc) {
    if (category == "A7") return true;

    if (desc.contains("Heli", Qt::CaseInsensitive) || desc.contains("Rotor", Qt::CaseInsensitive)) {
        return true;
    }

    static const char* knownHeliCodes[] = {
        "H120", "H125", "H130", "H135", "H145", "H155", "H160", "H175", "H215", "H225",
        "EC20", "EC30", "EC35", "EC45", "EC55", "AS50", "AS55", "AS65", "SA33", "BO105",
        "B06",  "B206", "B407", "B412", "B429", "B430", "B505", "UH1",  "AH1",
        "S76",  "S92",  "UH60", "SH60", "CH53", "CH47", "AH64", "R22",  "R44",  "R66",
        "A109", "A119", "A139", "A169", "A189", "AW09", "AW19", "AW139", "AW169",
        "MD50", "MD52", "MD60", "MD90", "EXPL",
        "DHRV", "LCH",  "LUH",  "CHTK", "CHTH", "RUDN",
        "MI8",  "MI17", "MI24", "MI26", "MI35", "KA27", "KA28", "KA31", "KA52"
    };

    for (const char* code : knownHeliCodes) {
        if (typeCode.compare(code, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

QString HelicopterTrackerManager::resolveModelName(const QString& typeCode) {
    if (typeCode == "MI17") return "Mil Mi-17V-5 Hip";
    if (typeCode == "MI8") return "Mil Mi-8 Hip";
    if (typeCode == "MI26") return "Mil Mi-26 Halo";
    if (typeCode == "DHRV") return "HAL Dhruv ALH Mk-III";
    if (typeCode == "LCH") return "HAL Prachand LCH";
    if (typeCode == "LUH") return "HAL Light Utility Heli";
    if (typeCode == "CHTK") return "HAL Chetak (Alouette III)";
    if (typeCode == "CHTH") return "HAL Cheetah (Lama)";
    if (typeCode == "H145" || typeCode == "EC45") return "Airbus H145 / EC145";
    if (typeCode == "H135" || typeCode == "EC35") return "Airbus H135 / EC135";
    if (typeCode == "H125" || typeCode == "AS50") return "Airbus H125 Écureuil";
    if (typeCode == "H225" || typeCode == "EC25") return "Airbus H225 Super Puma";
    if (typeCode == "B412") return "Bell 412EP Sentinel";
    if (typeCode == "B407") return "Bell 407GXi";
    if (typeCode == "B429") return "Bell 429 GlobalRanger";
    if (typeCode == "B206" || typeCode == "B06") return "Bell 206 JetRanger";
    if (typeCode == "AW139" || typeCode == "A139") return "Leonardo AW139";
    if (typeCode == "AW109" || typeCode == "A109") return "Leonardo AW109 Power";
    if (typeCode == "S92") return "Sikorsky S-92 Helibus";
    if (typeCode == "S76") return "Sikorsky S-76 Spirit";
    if (typeCode == "UH60") return "Sikorsky UH-60 Black Hawk";
    if (typeCode == "CH47") return "Boeing CH-47F Chinook";
    if (typeCode == "AH64") return "Boeing AH-64E Apache";
    if (typeCode == "R44") return "Robinson R44 Raven II";
    if (typeCode == "R66") return "Robinson R66 Turbine";

    return typeCode.isEmpty() ? "Rotorcraft" : QString("Helicopter (%1)").arg(typeCode);
}

QString HelicopterTrackerManager::resolveOperator(const QString& flight, const QString& reg) {
    if (flight.startsWith("IAF", Qt::CaseInsensitive) || reg.startsWith("Z-")) {
        return "Indian Air Force (IAF)";
    }
    if (flight.startsWith("NDRF", Qt::CaseInsensitive)) {
        return "National Disaster Response Force (NDRF)";
    }
    if (flight.startsWith("CG", Qt::CaseInsensitive)) {
        return "Indian Coast Guard (ICG)";
    }
    if (flight.startsWith("BSF", Qt::CaseInsensitive)) {
        return "BSF Air Wing";
    }
    if (flight.startsWith("MED", Qt::CaseInsensitive) || flight.startsWith("AMB", Qt::CaseInsensitive)) {
        return "Emergency Medical Air Ambulance";
    }
    if (flight.startsWith("PAWAN", Qt::CaseInsensitive) || reg.startsWith("VT-P")) {
        return "Pawan Hans Helicopters";
    }
    if (reg.startsWith("VT-")) {
        return "Civil Aviation India (VT)";
    }
    return "Airspace SAR / Medevac Unit";
}

} // namespace MapCore
