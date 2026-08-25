#include "WeatherForecastManager.h"
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <cmath>

namespace MapCore {

WeatherForecastManager::WeatherForecastManager(QObject* parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this)) {
    connect(networkManager, &QNetworkAccessManager::finished, this, &WeatherForecastManager::onReplyFinished);
}

void WeatherForecastManager::fetchForecast(double latitude, double longitude, const QString& locationName) {
    pendingLat = latitude;
    pendingLon = longitude;
    pendingLocationName = locationName.isEmpty() ? QString("%1° N, %2° E").arg(latitude, 0, 'f', 4).arg(longitude, 0, 'f', 4) : locationName;
    fetching = true;

    QUrl qurl("https://api.open-meteo.com/v1/forecast");
    QUrlQuery query;
    query.addQueryItem("latitude", QString::number(latitude, 'f', 4));
    query.addQueryItem("longitude", QString::number(longitude, 'f', 4));
    query.addQueryItem("current", "temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m,surface_pressure");
    query.addQueryItem("hourly", "temperature_2m,relative_humidity_2m,apparent_temperature,precipitation,precipitation_probability,cloud_cover,cloud_cover_low,cloud_cover_mid,cloud_cover_high,wind_speed_10m,weather_code,surface_pressure");
    query.addQueryItem("daily", "weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,precipitation_probability_max");
    query.addQueryItem("timezone", "auto");
    qurl.setQuery(query);

    qDebug() << "[WeatherForecastManager] Requesting Weather.com-style dataset:" << qurl.toString();

    QNetworkRequest request(qurl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "RedR-MapGIS/1.0");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
    sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);
    request.setSslConfiguration(sslConfig);

    QNetworkReply* reply = networkManager->get(request);
    connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>&) {
        reply->ignoreSslErrors();
    });
}

void WeatherForecastManager::onReplyFinished(QNetworkReply* reply) {
    fetching = false;
    if (!reply) return;

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        QString err = reply->errorString();
        qWarning() << "[WeatherForecastManager] Network error:" << err << "HTTP status:" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        emit fetchFailed(err);
        return;
    }

    QByteArray data = reply->readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[WeatherForecastManager] JSON parse error:" << parseErr.errorString();
        emit fetchFailed("Failed to parse weather forecast JSON response");
        return;
    }

    QJsonObject root = doc.object();
    if (root.contains("error") && root.value("error").toBool()) {
        QString reason = root.value("reason").toString("Unknown Open-Meteo error");
        qWarning() << "[WeatherForecastManager] API returned error:" << reason;
        emit fetchFailed(reason);
        return;
    }

    WeatherForecastData newForecast;
    newForecast.isValid = true;
    newForecast.latitude = root.value("latitude").toDouble(pendingLat);
    newForecast.longitude = root.value("longitude").toDouble(pendingLon);
    newForecast.elevation = root.value("elevation").toDouble(0.0);
    newForecast.timezone = root.value("timezone").toString("Asia/Kolkata");
    newForecast.locationName = pendingLocationName;

    // 1. Current Weather
    QJsonObject curObj = root.value("current").toObject();
    newForecast.current.temperatureC = curObj.value("temperature_2m").toDouble(25.0);
    newForecast.current.apparentTemperatureC = curObj.value("apparent_temperature").toDouble(newForecast.current.temperatureC);
    newForecast.current.relativeHumidity = curObj.value("relative_humidity_2m").toDouble(60.0);
    newForecast.current.precipitationMm = curObj.value("precipitation").toDouble(0.0);
    newForecast.current.windSpeedKmh = curObj.value("wind_speed_10m").toDouble(10.0);
    newForecast.current.surfacePressureHpa = curObj.value("surface_pressure").toDouble(1013.0);
    newForecast.current.weatherCode = curObj.value("weather_code").toInt(2);
    newForecast.current.weatherDesc = getWeatherDescription(newForecast.current.weatherCode);
    newForecast.current.weatherIcon = getWeatherIcon(newForecast.current.weatherCode);

    // 2. Hourly Weather (Next 24 to 168 hours)
    QJsonObject hourly = root.value("hourly").toObject();
    QJsonArray timeArr = hourly.value("time").toArray();
    QJsonArray tempArr = hourly.value("temperature_2m").toArray();
    QJsonArray appTempArr = hourly.value("apparent_temperature").toArray();
    QJsonArray rhArr = hourly.value("relative_humidity_2m").toArray();
    QJsonArray precipArr = hourly.value("precipitation").toArray();
    QJsonArray precipProbArr = hourly.value("precipitation_probability").toArray();
    QJsonArray wcodeArr = hourly.value("weather_code").toArray();
    QJsonArray ccArr = hourly.value("cloud_cover").toArray();
    QJsonArray ccLowArr = hourly.value("cloud_cover_low").toArray();
    QJsonArray ccMidArr = hourly.value("cloud_cover_mid").toArray();
    QJsonArray ccHighArr = hourly.value("cloud_cover_high").toArray();
    QJsonArray windArr = hourly.value("wind_speed_10m").toArray();
    QJsonArray pressArr = hourly.value("surface_pressure").toArray();

    int n = timeArr.size();
    newForecast.hourly.reserve(n);

    for (int i = 0; i < n; ++i) {
        HourlyWeather hw;
        hw.timeIso = timeArr.at(i).toString();
        hw.temperatureC = (i < tempArr.size()) ? tempArr.at(i).toDouble(0.0) : 0.0;
        hw.apparentTemperatureC = (i < appTempArr.size()) ? appTempArr.at(i).toDouble(hw.temperatureC) : hw.temperatureC;
        hw.relativeHumidity = (i < rhArr.size()) ? rhArr.at(i).toDouble(0.0) : 0.0;
        hw.precipitationMm = (i < precipArr.size()) ? precipArr.at(i).toDouble(0.0) : 0.0;
        hw.precipitationProbability = (i < precipProbArr.size()) ? precipProbArr.at(i).toDouble(0.0) : 0.0;
        hw.weatherCode = (i < wcodeArr.size()) ? wcodeArr.at(i).toInt(0) : 0;
        hw.weatherDesc = getWeatherDescription(hw.weatherCode);
        hw.weatherIcon = getWeatherIcon(hw.weatherCode);
        hw.cloudCoverPct = (i < ccArr.size()) ? ccArr.at(i).toDouble(0.0) : 0.0;
        hw.cloudCoverLow = (i < ccLowArr.size()) ? ccLowArr.at(i).toDouble(0.0) : 0.0;
        hw.cloudCoverMid = (i < ccMidArr.size()) ? ccMidArr.at(i).toDouble(0.0) : 0.0;
        hw.cloudCoverHigh = (i < ccHighArr.size()) ? ccHighArr.at(i).toDouble(0.0) : 0.0;
        hw.windSpeedKmh = (i < windArr.size()) ? windArr.at(i).toDouble(0.0) : 0.0;
        hw.surfacePressureHpa = (i < pressArr.size()) ? pressArr.at(i).toDouble(1013.0) : 1013.0;
        newForecast.hourly.push_back(hw);
    }

    // 3. Daily Weather (7-Day Horizon)
    QJsonObject daily = root.value("daily").toObject();
    QJsonArray dTimeArr = daily.value("time").toArray();
    QJsonArray dCodeArr = daily.value("weather_code").toArray();
    QJsonArray dMaxTempArr = daily.value("temperature_2m_max").toArray();
    QJsonArray dMinTempArr = daily.value("temperature_2m_min").toArray();
    QJsonArray dPrecipSumArr = daily.value("precipitation_sum").toArray();
    QJsonArray dPrecipProbArr = daily.value("precipitation_probability_max").toArray();

    int numDays = dTimeArr.size();
    newForecast.daily.reserve(numDays);

    for (int i = 0; i < numDays; ++i) {
        DailyWeather dw;
        dw.dateIso = dTimeArr.at(i).toString();
        QDate date = QDate::fromString(dw.dateIso, Qt::ISODate);
        if (i == 0) {
            dw.dayName = "Today";
            newForecast.current.tempMaxC = (i < dMaxTempArr.size()) ? dMaxTempArr.at(i).toDouble(newForecast.current.temperatureC) : newForecast.current.temperatureC;
            newForecast.current.tempMinC = (i < dMinTempArr.size()) ? dMinTempArr.at(i).toDouble(newForecast.current.temperatureC) : newForecast.current.temperatureC;
        } else if (i == 1) {
            dw.dayName = "Tomorrow";
        } else {
            dw.dayName = date.toString("ddd, MMM d");
        }
        dw.weatherCode = (i < dCodeArr.size()) ? dCodeArr.at(i).toInt(0) : 0;
        dw.weatherDesc = getWeatherDescription(dw.weatherCode);
        dw.weatherIcon = getWeatherIcon(dw.weatherCode);
        dw.tempMaxC = (i < dMaxTempArr.size()) ? dMaxTempArr.at(i).toDouble(0.0) : 0.0;
        dw.tempMinC = (i < dMinTempArr.size()) ? dMinTempArr.at(i).toDouble(0.0) : 0.0;
        dw.precipitationSumMm = (i < dPrecipSumArr.size()) ? dPrecipSumArr.at(i).toDouble(0.0) : 0.0;
        dw.precipitationProbabilityMax = (i < dPrecipProbArr.size()) ? dPrecipProbArr.at(i).toDouble(0.0) : 0.0;
        newForecast.daily.push_back(dw);
    }

    qDebug() << "[WeatherForecastManager] Successfully loaded full Weather.com dataset (" << newForecast.hourly.size() << "hours," << newForecast.daily.size() << "days) for" << newForecast.locationName;
    currentForecast = newForecast;
    emit forecastUpdated(currentForecast);
}

} // namespace MapCore
