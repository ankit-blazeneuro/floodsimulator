#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPainter>
#include <vector>
#include "../core/WeatherForecastManager.h"
#include "../core/TileCacheManager.h"

namespace MapUI {

class StatusDotWidget : public QWidget {
    Q_OBJECT
public:
    explicit StatusDotWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(10, 10);
    }
    void setConnected(bool connected) {
        if (isConnected != connected) {
            isConnected = connected;
            update();
        }
    }
    bool getConnected() const { return isConnected; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(isConnected ? QColor("#38BDF8") : QColor("#EF4444"));
        p.drawEllipse(0, 0, width(), height());
    }

private:
    bool isConnected = false;
};

class WeatherHudWidget : public QWidget {
    Q_OBJECT

public:
    explicit WeatherHudWidget(QWidget* parent = nullptr);
    ~WeatherHudWidget() override = default;

    void setForecast(const MapCore::WeatherForecastData& data);
    void setHourIndex(int hour);
    void setTileProvider(MapCore::OnlineTileProvider provider);

signals:
    void locationRequested(double lat, double lon, const QString& name);
    void refreshRequested();
    void fitAssamRequested();
    void fitIndiaRequested();
    void syncToggled(bool sync);
    void tileProviderChanged(MapCore::OnlineTileProvider provider);
    void hourSelected(int hour);

private slots:
    void onSearchSubmitted();

private:
    void setupUi();
    void updateDisplay();
    void updateHourlyStrip();
    void updateDailyForecast();

    MapCore::WeatherForecastData currentForecast;
    int currentHour = 0;

    // Search UI
    QLineEdit* searchInput;
    QPushButton* btnSearch;

    // Header UI
    StatusDotWidget* statusDot;
    QLabel* lblHeader;
    QLabel* lblTimestamp;
    QPushButton* btnRefresh;

    // Hero Today's Card (Weather.com style)
    QLabel* lblHeroLocation;
    QLabel* lblHeroTemp;
    QLabel* lblHeroCondition;
    QLabel* lblHeroHighLow;
    QLabel* lblHeroFeelsLike;
    QLabel* lblHeroPrecipProb;

    // Alert Banner
    QWidget* alertCard;
    QLabel* lblAlertText;

    // 24-Hour Hourly Horizontal Horizon Strip
    QWidget* hourlyStripContainer;
    QHBoxLayout* hourlyStripLayout;
    std::vector<QWidget*> hourlyCards;

    // 7-Day Daily Forecast List
    QWidget* dailyContainer;
    QVBoxLayout* dailyListLayout;
    std::vector<QWidget*> dailyCards;

    // Weather.com Bento Metrics Grid (6 Cards)
    QLabel* valTempFeels;
    QLabel* valWindGusts;
    QLabel* valHumidityDew;
    QLabel* valPrecipRate;
    QLabel* valCloudLayers;
    QLabel* valPressureElevation;

    // Quick Map Controls
    QPushButton* btnFitAssam;
    QPushButton* btnFitIndia;
    QComboBox* comboTileProvider;
};

} // namespace MapUI
