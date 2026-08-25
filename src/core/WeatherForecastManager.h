#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <vector>

namespace MapCore {

inline QString getWeatherDescription(int code) {
    switch (code) {
        case 0: return "Clear Sky";
        case 1: return "Mainly Clear";
        case 2: return "Partly Cloudy";
        case 3: return "Overcast";
        case 45: return "Foggy";
        case 48: return "Depositing Rime Fog";
        case 51: return "Light Drizzle";
        case 53: return "Moderate Drizzle";
        case 55: return "Dense Drizzle";
        case 61: return "Light Rain";
        case 63: return "Moderate Rain";
        case 65: return "Heavy Rain";
        case 71: return "Light Snow";
        case 73: return "Moderate Snow";
        case 75: return "Heavy Snow";
        case 80: return "Light Rain Showers";
        case 81: return "Moderate Rain Showers";
        case 82: return "Violent Rain Showers";
        case 95: return "Thunderstorm";
        case 96: return "Thunderstorm w/ Light Hail";
        case 99: return "Severe Thunderstorm w/ Hail";
        default: return "Partly Cloudy";
    }
}

inline QString getWeatherIcon(int code) {
    switch (code) {
        case 0: return "☀️";
        case 1: return "🌤️";
        case 2: return "⛅";
        case 3: return "☁️";
        case 45: case 48: return "🌫️";
        case 51: case 53: case 55: return "🌦️";
        case 61: case 63: return "🌧️";
        case 65: return "⛈️";
        case 71: case 73: case 75: return "🌨️";
        case 80: case 81: return "🌦️";
        case 82: return "🌧️";
        case 95: case 96: case 99: return "⛈️";
        default: return "⛅";
    }
}

struct HourlyWeather {
    QString timeIso;
    double temperatureC = 0.0;
    double apparentTemperatureC = 0.0;
    double relativeHumidity = 0.0;
    double precipitationMm = 0.0;
    double precipitationProbability = 0.0;
    int weatherCode = 0;
    QString weatherDesc;
    QString weatherIcon = "☀️";
    double cloudCoverPct = 0.0;
    double cloudCoverLow = 0.0;
    double cloudCoverMid = 0.0;
    double cloudCoverHigh = 0.0;
    double windSpeedKmh = 0.0;
    double surfacePressureHpa = 1013.25;
};

struct DailyWeather {
    QString dateIso;
    QString dayName;
    double tempMaxC = 0.0;
    double tempMinC = 0.0;
    double precipitationSumMm = 0.0;
    double precipitationProbabilityMax = 0.0;
    int weatherCode = 0;
    QString weatherDesc;
    QString weatherIcon = "☀️";
};

struct CurrentWeather {
    double temperatureC = 0.0;
    double apparentTemperatureC = 0.0; // Feels Like
    double tempMaxC = 0.0;
    double tempMinC = 0.0;
    double relativeHumidity = 0.0;
    double precipitationMm = 0.0;
    double windSpeedKmh = 0.0;
    double surfacePressureHpa = 1013.25;
    int weatherCode = 0;
    QString weatherDesc = "Partly Cloudy";
    QString weatherIcon = "⛅";
    double uvIndex = 4.0;
};

struct WeatherForecastData {
    bool isValid = false;
    double latitude = 26.1445;
    double longitude = 91.7362;
    double elevation = 55.0;
    QString timezone = "Asia/Kolkata";
    QString locationName = "Guwahati, Assam";
    CurrentWeather current;
    std::vector<HourlyWeather> hourly;
    std::vector<DailyWeather> daily;

    const HourlyWeather* getHour(int hourIndex) const {
        if (hourly.empty()) return nullptr;
        if (hourIndex < 0) return &hourly.front();
        if (static_cast<size_t>(hourIndex) >= hourly.size()) return &hourly.back();
        return &hourly[hourIndex];
    }
};

class WeatherForecastManager : public QObject {
    Q_OBJECT

public:
    explicit WeatherForecastManager(QObject* parent = nullptr);
    ~WeatherForecastManager() override = default;

    void fetchForecast(double latitude = 26.1445, double longitude = 91.7362, const QString& locationName = "");
    const WeatherForecastData& getForecast() const { return currentForecast; }
    bool isLoading() const { return fetching; }

signals:
    void forecastUpdated(const WeatherForecastData& data);
    void fetchFailed(const QString& errorMessage);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* networkManager = nullptr;
    WeatherForecastData currentForecast;
    bool fetching = false;
    double pendingLat = 26.1445;
    double pendingLon = 91.7362;
    QString pendingLocationName;
};

} // namespace MapCore
