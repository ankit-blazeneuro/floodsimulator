#include "WeatherHudWidget.h"
#include <QDateTime>
#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>
#include <cmath>
#include <algorithm>

namespace MapUI {

WeatherHudWidget::WeatherHudWidget(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void WeatherHudWidget::setupUi() {
    setObjectName("weatherSidebarRoot");
    setMinimumWidth(320);
    setStyleSheet(R"(
        QWidget#weatherSidebarRoot {
            background-color: #121215;
            border-right: 1px solid #27272A;
        }
        QScrollArea {
            background-color: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: #18181B;
            width: 6px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3F3F46;
            min-height: 20px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: #71717A;
        }
        QScrollBar:horizontal {
            background: #18181B;
            height: 6px;
            margin: 0px;
        }
        QScrollBar::handle:horizontal {
            background: #3F3F46;
            min-width: 20px;
            border-radius: 3px;
        }
        QScrollBar::add-line, QScrollBar::sub-line {
            width: 0px;
            height: 0px;
        }
        QLabel {
            font-family: 'Segoe UI', Inter, -apple-system, sans-serif;
            color: #F4F4F5;
            background: transparent;
            border: none;
        }
        QLineEdit {
            background-color: #27272A;
            color: #F4F4F5;
            border: 1px solid #3F3F46;
            border-radius: 6px;
            padding: 6px 10px;
            font-family: 'Segoe UI', Inter, sans-serif;
            font-size: 11px;
        }
        QLineEdit:focus {
            border: 1px solid #38BDF8;
            background-color: #18181B;
        }
        QPushButton.actionBtn {
            background-color: #27272A;
            color: #E4E4E7;
            border: 1px solid #3F3F46;
            border-radius: 6px;
            font-family: 'Segoe UI', Inter, sans-serif;
            font-size: 11px;
            font-weight: 600;
            padding: 6px 12px;
        }
        QPushButton.actionBtn:hover {
            background-color: #38BDF8;
            color: #09090B;
            border-color: #38BDF8;
        }
        QPushButton.presetBtn {
            background-color: #27272A;
            color: #D4D4D8;
            border: 1px solid #3F3F46;
            border-radius: 4px;
            font-family: 'Segoe UI', Inter, sans-serif;
            font-size: 10px;
            font-weight: 500;
            padding: 3px 7px;
        }
        QPushButton.presetBtn:hover {
            background-color: rgba(56, 189, 248, 0.20);
            color: #38BDF8;
            border: 1px solid #38BDF8;
        }
        QComboBox {
            background-color: #27272A;
            color: #F4F4F5;
            border: 1px solid #3F3F46;
            border-radius: 6px;
            padding: 4px 8px;
            font-size: 11px;
        }
        QComboBox:hover {
            border-color: #38BDF8;
        }
        QComboBox QAbstractItemView {
            background-color: #18181B;
            color: #F4F4F5;
            selection-background-color: #0284C7;
            selection-color: #FFFFFF;
            border: 1px solid #3F3F46;
        }
    )");

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // Main Vertical Scroll Area
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* contentWidget = new QWidget(scrollArea);
    auto* layout = new QVBoxLayout(contentWidget);
    layout->setContentsMargins(12, 12, 12, 16);
    layout->setSpacing(12);

    // ==========================================
    // 1. Header Row: Title + Status Dot + Refresh
    // ==========================================
    auto* headerRow = new QHBoxLayout();
    headerRow->setContentsMargins(0, 0, 0, 0);
    headerRow->setSpacing(8);

    lblHeader = new QLabel("🌦️ Weather Intelligence", contentWidget);
    lblHeader->setStyleSheet("font-size: 13px; font-weight: bold; color: #38BDF8;");
    headerRow->addWidget(lblHeader, 1);

    statusDot = new StatusDotWidget(contentWidget);
    headerRow->addWidget(statusDot, 0, Qt::AlignVCenter);

    btnRefresh = new QPushButton("🔄", contentWidget);
    btnRefresh->setFixedSize(24, 24);
    btnRefresh->setToolTip("Refresh Weather Data from Open-Meteo");
    btnRefresh->setStyleSheet("QPushButton { background-color: #27272A; color: #D4D4D8; border: 1px solid #3F3F46; border-radius: 4px; font-size: 10px; } QPushButton:hover { background-color: #3F3F46; color: #FFFFFF; }");
    connect(btnRefresh, &QPushButton::clicked, this, [this]() {
        emit refreshRequested();
    });
    headerRow->addWidget(btnRefresh, 0);

    layout->addLayout(headerRow);

    // ==========================================
    // 2. Search Box
    // ==========================================
    auto* searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(6);

    searchInput = new QLineEdit(contentWidget);
    searchInput->setPlaceholderText("Search city, region, or lat, lon...");
    searchInput->setClearButtonEnabled(true);
    connect(searchInput, &QLineEdit::returnPressed, this, &WeatherHudWidget::onSearchSubmitted);
    searchRow->addWidget(searchInput, 1);

    btnSearch = new QPushButton("Search", contentWidget);
    btnSearch->setObjectName("btnSearch");
    btnSearch->setStyleSheet("QPushButton { background-color: #27272A; color: #E4E4E7; border: 1px solid #3F3F46; border-radius: 6px; font-size: 11px; font-weight: 600; padding: 6px 10px; } QPushButton:hover { background-color: #38BDF8; color: #09090B; }");
    connect(btnSearch, &QPushButton::clicked, this, &WeatherHudWidget::onSearchSubmitted);
    searchRow->addWidget(btnSearch, 0);

    layout->addLayout(searchRow);

    // ==========================================
    // 3. Quick Region Preset Chips
    // ==========================================
    auto* chipGrid = new QGridLayout();
    chipGrid->setContentsMargins(0, 0, 0, 0);
    chipGrid->setSpacing(4);

    struct PresetCity { const char* name; double lat; double lon; };
    static const PresetCity presets[] = {
        {"Guwahati", 26.1445, 91.7362},
        {"Tezpur", 26.6338, 92.7926},
        {"Dibrugarh", 27.4728, 94.9120},
        {"Silchar", 24.8333, 92.7789},
        {"Delhi", 28.6139, 77.2090},
        {"Mumbai", 19.0760, 72.8777},
        {"Kolkata", 22.5726, 88.3639},
        {"Bengaluru", 12.9716, 77.5946}
    };

    for (int i = 0; i < 8; ++i) {
        auto* btn = new QPushButton(presets[i].name, contentWidget);
        btn->setProperty("class", "presetBtn");
        double lat = presets[i].lat;
        double lon = presets[i].lon;
        QString name = QString("%1, %2").arg(presets[i].name).arg(i < 4 ? "Assam" : "India");
        connect(btn, &QPushButton::clicked, this, [this, lat, lon, name]() {
            emit locationRequested(lat, lon, name);
        });
        chipGrid->addWidget(btn, i / 4, i % 4);
    }
    layout->addLayout(chipGrid);

    // ==========================================
    // 4. Weather.com Hero Current Weather Card
    // ==========================================
    auto* heroCard = new QWidget(contentWidget);
    heroCard->setStyleSheet(R"(
        QWidget {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #1E293B, stop:1 #0F172A);
            border: 1px solid #334155;
            border-radius: 10px;
        }
    )");
    auto* heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(14, 14, 14, 14);
    heroLayout->setSpacing(6);

    lblHeroLocation = new QLabel("📍 Guwahati, Assam", heroCard);
    lblHeroLocation->setStyleSheet("font-size: 13px; font-weight: 700; color: #F8FAFC;");
    heroLayout->addWidget(lblHeroLocation);

    auto* heroMainRow = new QHBoxLayout();
    heroMainRow->setContentsMargins(0, 4, 0, 4);

    lblHeroTemp = new QLabel("29°", heroCard);
    lblHeroTemp->setStyleSheet("font-size: 40px; font-weight: 800; color: #FFFFFF;");
    heroMainRow->addWidget(lblHeroTemp, 0, Qt::AlignVCenter);

    auto* heroCondCol = new QVBoxLayout();
    heroCondCol->setSpacing(2);

    lblHeroCondition = new QLabel("⛅ Partly Cloudy", heroCard);
    lblHeroCondition->setStyleSheet("font-size: 13px; font-weight: 700; color: #38BDF8;");
    heroCondCol->addWidget(lblHeroCondition);

    lblHeroFeelsLike = new QLabel("Feels like 33°C", heroCard);
    lblHeroFeelsLike->setStyleSheet("font-size: 11px; color: #94A3B8; font-weight: 500;");
    heroCondCol->addWidget(lblHeroFeelsLike);

    heroMainRow->addLayout(heroCondCol, 1);
    heroLayout->addLayout(heroMainRow);

    lblHeroHighLow = new QLabel("Day 34° • Night 24° · Precipitation: 0.0 mm", heroCard);
    lblHeroHighLow->setStyleSheet("font-size: 11px; color: #CBD5E1; font-weight: 500;");
    heroLayout->addWidget(lblHeroHighLow);

    layout->addWidget(heroCard);

    // Alert Banner (Shown on high precipitation / severe storms)
    alertCard = new QWidget(contentWidget);
    alertCard->setStyleSheet("background-color: rgba(225, 29, 72, 0.20); border: 1px solid rgba(244, 63, 94, 0.50); border-radius: 8px;");
    auto* alertLayout = new QHBoxLayout(alertCard);
    alertLayout->setContentsMargins(10, 8, 10, 8);
    lblAlertText = new QLabel(alertCard);
    lblAlertText->setStyleSheet("color: #FECDD3; font-size: 11px; font-weight: 600;");
    lblAlertText->setWordWrap(true);
    alertLayout->addWidget(lblAlertText);
    alertCard->hide();
    layout->addWidget(alertCard);

    // ==========================================
    // 5. Weather.com Hourly Forecast Horizontal Strip (24-Hour Horizon)
    // ==========================================
    auto* lblHourlyHeader = new QLabel("🕒 Hourly Forecast (24-Hour Horizon)", contentWidget);
    lblHourlyHeader->setStyleSheet("font-size: 12px; font-weight: 700; color: #E4E4E7;");
    layout->addWidget(lblHourlyHeader);

    auto* hourlyScroll = new QScrollArea(contentWidget);
    hourlyScroll->setFixedHeight(105);
    hourlyScroll->setWidgetResizable(true);
    hourlyScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    hourlyScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    hourlyStripContainer = new QWidget(hourlyScroll);
    hourlyStripLayout = new QHBoxLayout(hourlyStripContainer);
    hourlyStripLayout->setContentsMargins(0, 0, 0, 0);
    hourlyStripLayout->setSpacing(6);
    hourlyScroll->setWidget(hourlyStripContainer);

    layout->addWidget(hourlyScroll);

    // ==========================================
    // 6. Weather.com 7-Day Extended Daily Forecast
    // ==========================================
    auto* lblDailyHeader = new QLabel("📅 7-Day Extended Daily Outlook", contentWidget);
    lblDailyHeader->setStyleSheet("font-size: 12px; font-weight: 700; color: #E4E4E7;");
    layout->addWidget(lblDailyHeader);

    dailyContainer = new QWidget(contentWidget);
    dailyListLayout = new QVBoxLayout(dailyContainer);
    dailyListLayout->setContentsMargins(0, 0, 0, 0);
    dailyListLayout->setSpacing(4);
    layout->addWidget(dailyContainer);

    // ==========================================
    // 7. Weather.com Today's Details Bento Grid (6 Key Metric Cards)
    // ==========================================
    auto* lblDetailsHeader = new QLabel("📊 Atmospheric Conditions", contentWidget);
    lblDetailsHeader->setStyleSheet("font-size: 12px; font-weight: 700; color: #E4E4E7;");
    layout->addWidget(lblDetailsHeader);

    auto* bentoGrid = new QGridLayout();
    bentoGrid->setContentsMargins(0, 0, 0, 0);
    bentoGrid->setSpacing(8);

    auto createMetricCard = [](const QString& title, QLabel*& valLabel) -> QWidget* {
        auto* card = new QWidget();
        card->setStyleSheet("QWidget { background-color: #1E1E24; border: 1px solid #2E2E38; border-radius: 8px; }");
        auto* l = new QVBoxLayout(card);
        l->setContentsMargins(10, 8, 10, 8);
        l->setSpacing(2);

        auto* lblT = new QLabel(title, card);
        lblT->setStyleSheet("font-size: 10px; font-weight: 600; color: #94A3B8;");
        l->addWidget(lblT);

        valLabel = new QLabel("--", card);
        valLabel->setStyleSheet("font-size: 12px; font-weight: 700; color: #F4F4F5;");
        l->addWidget(valLabel);

        return card;
    };

    bentoGrid->addWidget(createMetricCard("🌡️ Feels Like & Index", valTempFeels), 0, 0);
    bentoGrid->addWidget(createMetricCard("💨 Wind & Airflow", valWindGusts), 0, 1);
    bentoGrid->addWidget(createMetricCard("💧 Humidity & Dew Point", valHumidityDew), 1, 0);
    bentoGrid->addWidget(createMetricCard("🌧️ Precipitation Rate", valPrecipRate), 1, 1);
    bentoGrid->addWidget(createMetricCard("☁️ Cloud Distribution", valCloudLayers), 2, 0);
    bentoGrid->addWidget(createMetricCard("🧭 Pressure & Elevation", valPressureElevation), 2, 1);

    layout->addLayout(bentoGrid);

    // ==========================================
    // 8. Map Viewport Preset Controls
    // ==========================================
    auto* lblMapCtrlHeader = new QLabel("🗺️ Viewport Camera Presets", contentWidget);
    lblMapCtrlHeader->setStyleSheet("font-size: 12px; font-weight: 700; color: #E4E4E7;");
    layout->addWidget(lblMapCtrlHeader);

    auto* btnRow = new QHBoxLayout();
    btnRow->setContentsMargins(0, 0, 0, 0);
    btnRow->setSpacing(6);

    btnFitAssam = new QPushButton("🗺️ Fit Assam", contentWidget);
    btnFitAssam->setStyleSheet("QPushButton { background-color: #27272A; color: #D4D4D8; border: 1px solid #3F3F46; border-radius: 6px; font-size: 11px; font-weight: 600; padding: 6px; } QPushButton:hover { background-color: #38BDF8; color: #09090B; }");
    connect(btnFitAssam, &QPushButton::clicked, this, [this]() {
        emit fitAssamRequested();
    });
    btnRow->addWidget(btnFitAssam);

    btnFitIndia = new QPushButton("🇮🇳 Fit India", contentWidget);
    btnFitIndia->setStyleSheet("QPushButton { background-color: #27272A; color: #D4D4D8; border: 1px solid #3F3F46; border-radius: 6px; font-size: 11px; font-weight: 600; padding: 6px; } QPushButton:hover { background-color: #38BDF8; color: #09090B; }");
    connect(btnFitIndia, &QPushButton::clicked, this, [this]() {
        emit fitIndiaRequested();
    });
    btnRow->addWidget(btnFitIndia);

    layout->addLayout(btnRow);

    // Base Tile Theme Selector
    comboTileProvider = new QComboBox(contentWidget);
    comboTileProvider->addItem("CartoDB Dark Theme", static_cast<int>(MapCore::OnlineTileProvider::OpenStreetMap_Dark));
    comboTileProvider->addItem("OpenStreetMap Standard", static_cast<int>(MapCore::OnlineTileProvider::OpenStreetMap_Standard));
    comboTileProvider->addItem("CartoDB Voyager Theme", static_cast<int>(MapCore::OnlineTileProvider::OpenStreetMap_Voyager));
    connect(comboTileProvider, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        auto prov = static_cast<MapCore::OnlineTileProvider>(comboTileProvider->itemData(idx).toInt());
        emit tileProviderChanged(prov);
    });
    layout->addWidget(comboTileProvider);

    layout->addStretch(1);

    scrollArea->setWidget(contentWidget);
    rootLayout->addWidget(scrollArea);
}

void WeatherHudWidget::onSearchSubmitted() {
    QString q = searchInput->text().trimmed();
    if (q.isEmpty()) return;

    if (q.contains(',')) {
        QStringList parts = q.split(',');
        if (parts.size() == 2) {
            bool okLat = false, okLon = false;
            double lat = parts[0].trimmed().toDouble(&okLat);
            double lon = parts[1].trimmed().toDouble(&okLon);
            if (okLat && okLon) {
                QString name = QString("Coord (%1° N, %2° E)").arg(lat, 0, 'f', 2).arg(lon, 0, 'f', 2);
                emit locationRequested(lat, lon, name);
                return;
            }
        }
    }

    struct CityCoord {
        const char* name;
        double lat;
        double lon;
    };

    static const CityCoord cities[] = {
        {"guwahati", 26.1445, 91.7362},
        {"tezpur", 26.6338, 92.7926},
        {"dibrugarh", 27.4728, 94.9120},
        {"silchar", 24.8333, 92.7789},
        {"jorhat", 26.7509, 94.2037},
        {"nagaon", 26.3452, 92.6840},
        {"shillong", 25.5788, 91.8933},
        {"itanagar", 27.0844, 93.6053},
        {"imphal", 24.8170, 93.9368},
        {"aizawl", 23.7271, 92.7176},
        {"kohima", 25.6751, 94.1086},
        {"agartala", 23.8315, 91.2868},
        {"gangtok", 27.3389, 88.6065},
        {"delhi", 28.6139, 77.2090},
        {"new delhi", 28.6139, 77.2090},
        {"mumbai", 19.0760, 72.8777},
        {"kolkata", 22.5726, 88.3639},
        {"chennai", 13.0827, 80.2707},
        {"bengaluru", 12.9716, 77.5946},
        {"bangalore", 12.9716, 77.5946},
        {"hyderabad", 17.3850, 78.4867},
        {"ahmedabad", 23.0225, 72.5714},
        {"pune", 18.5204, 73.8567},
        {"jaipur", 26.9124, 75.7873},
        {"patna", 25.5941, 85.1376},
        {"srinagar", 34.0837, 74.7973},
        {"kochi", 9.9312, 76.2673},
        {"cochin", 9.9312, 76.2673}
    };

    QString lowerQ = q.toLower();
    for (const auto& city : cities) {
        if (lowerQ.contains(city.name)) {
            QString formattedName = q;
            formattedName[0] = formattedName[0].toUpper();
            emit locationRequested(city.lat, city.lon, formattedName);
            return;
        }
    }

    emit locationRequested(26.1445, 91.7362, q);
}

void WeatherHudWidget::setForecast(const MapCore::WeatherForecastData& data) {
    currentForecast = data;
    updateDisplay();
    updateHourlyStrip();
    updateDailyForecast();
}

void WeatherHudWidget::setHourIndex(int hour) {
    currentHour = hour;
    updateDisplay();

    for (size_t i = 0; i < hourlyCards.size(); ++i) {
        if (hourlyCards[i]) {
            if (static_cast<int>(i) == currentHour) {
                hourlyCards[i]->setStyleSheet("QWidget { background-color: #0284C7; border: 1.5px solid #38BDF8; border-radius: 8px; }");
            } else {
                hourlyCards[i]->setStyleSheet("QWidget { background-color: #1E1E24; border: 1px solid #2E2E38; border-radius: 8px; } QWidget:hover { background-color: #27272A; }");
            }
        }
    }
}

void WeatherHudWidget::setTileProvider(MapCore::OnlineTileProvider provider) {
    for (int i = 0; i < comboTileProvider->count(); ++i) {
        if (comboTileProvider->itemData(i).toInt() == static_cast<int>(provider)) {
            comboTileProvider->blockSignals(true);
            comboTileProvider->setCurrentIndex(i);
            comboTileProvider->blockSignals(false);
            break;
        }
    }
}

void WeatherHudWidget::updateDisplay() {
    bool connected = currentForecast.isValid && !currentForecast.hourly.empty();
    if (statusDot) {
        statusDot->setConnected(connected);
    }

    if (!connected) {
        lblHeroLocation->setText("📍 Guwahati, Assam");
        lblHeroTemp->setText("--°");
        lblHeroCondition->setText("Offline");
        lblHeroFeelsLike->setText("Feels like --°C");
        lblHeroHighLow->setText("Day --° • Night --°");
        valTempFeels->setText("--");
        valWindGusts->setText("--");
        valHumidityDew->setText("--");
        valPrecipRate->setText("--");
        valCloudLayers->setText("--");
        valPressureElevation->setText("--");
        alertCard->hide();
        return;
    }

    const auto* hw = currentForecast.getHour(currentHour);
    if (!hw) return;

    lblHeroLocation->setText(QString("📍 %1").arg(currentForecast.locationName));
    lblHeroTemp->setText(QString("%1°").arg(qRound(hw->temperatureC)));
    lblHeroCondition->setText(QString("%1 %2").arg(hw->weatherIcon).arg(hw->weatherDesc));
    lblHeroFeelsLike->setText(QString("Feels like %1°C").arg(qRound(hw->apparentTemperatureC)));

    double maxT = currentForecast.current.tempMaxC;
    double minT = currentForecast.current.tempMinC;
    lblHeroHighLow->setText(QString("Day %1° • Night %2° · Rain Chance: %3%")
                            .arg(qRound(maxT))
                            .arg(qRound(minT))
                            .arg(qRound(hw->precipitationProbability)));

    // Bento Atmospheric Metrics
    valTempFeels->setText(QString("%1°C (Feels %2°C)")
                          .arg(hw->temperatureC, 0, 'f', 1)
                          .arg(hw->apparentTemperatureC, 0, 'f', 1));

    QString windCategory = (hw->windSpeedKmh < 10.0) ? "Light" : (hw->windSpeedKmh < 25.0 ? "Moderate" : (hw->windSpeedKmh < 45.0 ? "Strong" : "Gale"));
    valWindGusts->setText(QString("%1 km/h · %2").arg(hw->windSpeedKmh, 0, 'f', 1).arg(windCategory));

    double dewPoint = hw->temperatureC - ((100.0 - hw->relativeHumidity) / 5.0);
    valHumidityDew->setText(QString("%1% RH (Dew %2°C)")
                            .arg(qRound(hw->relativeHumidity))
                            .arg(dewPoint, 0, 'f', 1));

    valPrecipRate->setText(QString("%1 mm/h (%2% chance)")
                           .arg(hw->precipitationMm, 0, 'f', 1)
                           .arg(qRound(hw->precipitationProbability)));

    valCloudLayers->setText(QString("Total %1% (L:%2% M:%3% H:%4%)")
                            .arg(qRound(hw->cloudCoverPct))
                            .arg(qRound(hw->cloudCoverLow))
                            .arg(qRound(hw->cloudCoverMid))
                            .arg(qRound(hw->cloudCoverHigh)));

    valPressureElevation->setText(QString("%1 hPa · %2m MSL")
                                  .arg(qRound(hw->surfacePressureHpa))
                                  .arg(qRound(currentForecast.elevation)));

    // Alert Banner for Heavy Rain / Thunderstorms
    if (hw->precipitationMm > 5.0 || hw->weatherCode >= 95) {
        lblAlertText->setText(QString("⚠️ Severe Weather Alert: %1 with %2 mm/h precipitation. Elevated runoff and flood watch.")
                             .arg(hw->weatherDesc)
                             .arg(hw->precipitationMm, 0, 'f', 1));
        alertCard->show();
    } else {
        alertCard->hide();
    }
}

void WeatherHudWidget::updateHourlyStrip() {
    while (QLayoutItem* item = hourlyStripLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
    hourlyCards.clear();

    if (!currentForecast.isValid || currentForecast.hourly.empty()) return;

    int totalHours = std::min(24, static_cast<int>(currentForecast.hourly.size()));
    hourlyCards.reserve(totalHours);

    for (int h = 0; h < totalHours; ++h) {
        const auto& hw = currentForecast.hourly[h];
        auto* card = new QPushButton(hourlyStripContainer);
        card->setFixedWidth(68);
        card->setFixedHeight(88);
        card->setCursor(Qt::PointingHandCursor);

        bool isCurrent = (h == currentHour);
        card->setStyleSheet(isCurrent
            ? "QPushButton { background-color: #0284C7; border: 1.5px solid #38BDF8; border-radius: 8px; text-align: center; } QPushButton:hover { background-color: #0369A1; }"
            : "QPushButton { background-color: #1E1E24; border: 1px solid #2E2E38; border-radius: 8px; text-align: center; } QPushButton:hover { background-color: #27272A; border-color: #38BDF8; }");

        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(4, 6, 4, 6);
        cardLayout->setSpacing(2);
        cardLayout->setAlignment(Qt::AlignCenter);

        QString timeLabel = (h == 0) ? "Now" : QString("%1:00").arg(h % 24);
        auto* lblTime = new QLabel(timeLabel, card);
        lblTime->setAlignment(Qt::AlignCenter);
        lblTime->setStyleSheet("font-size: 10px; font-weight: 600; color: #94A3B8;");
        cardLayout->addWidget(lblTime);

        auto* lblIcon = new QLabel(hw.weatherIcon, card);
        lblIcon->setAlignment(Qt::AlignCenter);
        lblIcon->setStyleSheet("font-size: 16px;");
        cardLayout->addWidget(lblIcon);

        auto* lblTemp = new QLabel(QString("%1°").arg(qRound(hw.temperatureC)), card);
        lblTemp->setAlignment(Qt::AlignCenter);
        lblTemp->setStyleSheet("font-size: 11px; font-weight: 700; color: #F4F4F5;");
        cardLayout->addWidget(lblTemp);

        QString rainStr = (hw.precipitationProbability > 0) ? QString("🌧️ %1%").arg(qRound(hw.precipitationProbability)) : "";
        auto* lblRain = new QLabel(rainStr, card);
        lblRain->setAlignment(Qt::AlignCenter);
        lblRain->setStyleSheet("font-size: 9px; font-weight: 600; color: #38BDF8;");
        cardLayout->addWidget(lblRain);

        connect(card, &QPushButton::clicked, this, [this, h]() {
            emit hourSelected(h);
        });

        hourlyStripLayout->addWidget(card);
        hourlyCards.push_back(card);
    }
}

void WeatherHudWidget::updateDailyForecast() {
    while (QLayoutItem* item = dailyListLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->deleteLater();
        }
        delete item;
    }
    dailyCards.clear();

    if (!currentForecast.isValid || currentForecast.daily.empty()) return;

    for (const auto& dw : currentForecast.daily) {
        auto* row = new QWidget(dailyContainer);
        row->setStyleSheet("QWidget { background-color: #1E1E24; border: 1px solid #2E2E38; border-radius: 8px; } QWidget:hover { background-color: #27272A; }");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(10, 8, 10, 8);
        rowLayout->setSpacing(8);

        auto* lblDay = new QLabel(dw.dayName, row);
        lblDay->setFixedWidth(80);
        lblDay->setStyleSheet("font-size: 11px; font-weight: 600; color: #E4E4E7;");
        rowLayout->addWidget(lblDay);

        auto* lblCond = new QLabel(QString("%1 %2").arg(dw.weatherIcon).arg(dw.weatherDesc), row);
        lblCond->setStyleSheet("font-size: 11px; font-weight: 500; color: #94A3B8;");
        rowLayout->addWidget(lblCond, 1);

        if (dw.precipitationProbabilityMax > 15.0) {
            auto* lblPrecip = new QLabel(QString("🌧️ %1%").arg(qRound(dw.precipitationProbabilityMax)), row);
            lblPrecip->setStyleSheet("font-size: 10px; font-weight: 600; color: #38BDF8;");
            rowLayout->addWidget(lblPrecip);
        }

        auto* lblTempRange = new QLabel(QString("%1°  ·  %2°").arg(qRound(dw.tempMinC)).arg(qRound(dw.tempMaxC)), row);
        lblTempRange->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lblTempRange->setStyleSheet("font-size: 11px; font-weight: 700; color: #F4F4F5;");
        rowLayout->addWidget(lblTempRange);

        dailyListLayout->addWidget(row);
        dailyCards.push_back(row);
    }
}

} // namespace MapUI
