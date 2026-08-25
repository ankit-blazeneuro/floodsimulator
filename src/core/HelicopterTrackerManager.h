#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QPointF>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <vector>
#include <map>

namespace MapCore {

struct HelicopterTrack {
    QString hex;            // ICAO 24-bit hex identifier
    QString flight;         // Callsign (e.g. VT-HDC, IAF_RESCUE, MEDEVAC)
    QString registration;   // Aircraft tail number (e.g. VT-HDC)
    QString typeCode;       // Aircraft type (e.g. H145, B412, DHRV, MI17)
    QString modelName;      // Full name (e.g. Airbus H145, HAL Dhruv)
    QString operatorName;   // Operator (e.g. IAF SAR Unit, Pawan Hans, Air Ambulance)
    QString category;       // Category description (e.g. Rotorcraft / Helicopter)
    QString source = "ADS-B / OpenSky"; // Data feed source
    double lat = 0.0;
    double lon = 0.0;
    double altitudeFt = 0.0;
    double altitudeMeters = 0.0;
    double groundSpeedKnots = 0.0;
    double groundSpeedKmh = 0.0;
    double trackHeading = 0.0;
    double verticalRateFpm = 0.0;
    QString squawk = "7000";
    QString emergency = "none";
    double seenSecondsAgo = 0.0;
    QDateTime lastUpdated;
    bool isHelicopter = true;
    bool isSelected = false;
    std::vector<QPointF> trailHistory;
};

class HelicopterTrackerManager : public QObject {
    Q_OBJECT

public:
    explicit HelicopterTrackerManager(QObject* parent = nullptr);
    ~HelicopterTrackerManager() override;

    void startTracking(int intervalMs = 3000);
    void stopTracking();
    bool isTracking() const;

    void setTrackingCenter(double lat, double lon, double radiusNmi = 250.0);
    const std::vector<HelicopterTrack>& getHelicopters() const { return cachedHelicopters; }
    const HelicopterTrack* getHelicopter(const QString& hex) const;

public slots:
    void fetchLiveTelemetry();
    void fetchOpenSkyTelemetry();

signals:
    void helicoptersUpdated(const std::vector<HelicopterTrack>& list);
    void helicopterSelected(const HelicopterTrack& heli);
    void trackingStatusChanged(bool active, int count, const QString& statusMsg);

private slots:
    void onNetworkReply(QNetworkReply* reply);
    void onOpenSkyReply(QNetworkReply* reply);
    void onPollTimerTimeout();

private:
    void parseAdsbPayload(const QByteArray& data);
    void parseOpenSkyPayload(const QByteArray& data);
    void mergeHelicopter(const HelicopterTrack& heli);
    void generateSimulatedSARFleetIfNeeded();
    static bool isRotorcraftType(const QString& typeCode, const QString& category, const QString& desc);
    static QString resolveModelName(const QString& typeCode);
    static QString resolveOperator(const QString& flight, const QString& reg);

    QNetworkAccessManager* networkManager = nullptr;
    QTimer* pollTimer = nullptr;
    double centerLat = 26.14; // Default Assam center
    double centerLon = 91.73;
    double radiusNmi = 250.0;
    bool requestInProgress = false;
    bool openSkyInProgress = false;

    std::vector<HelicopterTrack> cachedHelicopters;
    std::map<QString, std::vector<QPointF>> flightTrails;
    int consecutiveFailures = 0;
};

} // namespace MapCore
