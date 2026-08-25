#include "HelicopterDetailsPanel.h"
#include <QFrame>
#include <QGraphicsDropShadowEffect>

namespace MapUI {

HelicopterDetailsPanel::HelicopterDetailsPanel(QWidget* parent)
    : QWidget(parent) {
    setupUi();
}

void HelicopterDetailsPanel::setupUi() {
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet("background-color: #141418; border-left: 1px solid #27272A;");
    setMinimumWidth(320);
    setMaximumWidth(380);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(14, 14, 14, 14);
    mainLayout->setSpacing(12);

    // ==========================================
    // 1. Header with Live Badge & Close Button
    // ==========================================
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);

    auto* lblTitle = new QLabel("🚁 Helicopter SAR", this);
    lblTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #FBBF24;");
    headerLayout->addWidget(lblTitle);

    headerLayout->addStretch(1);

    lblHeaderStatus = new QLabel("● 3s ADS-B", this);
    lblHeaderStatus->setStyleSheet("font-size: 10px; font-weight: 700; color: #10B981; background-color: #064E3B; border-radius: 4px; padding: 2px 6px;");
    headerLayout->addWidget(lblHeaderStatus);

    btnClose = new QPushButton("✕", this);
    btnClose->setFixedSize(22, 22);
    btnClose->setStyleSheet("QPushButton { background-color: #27272A; color: #A1A1AA; border: none; border-radius: 4px; font-weight: bold; font-size: 11px; } QPushButton:hover { background-color: #EF4444; color: #FFFFFF; }");
    connect(btnClose, &QPushButton::clicked, this, &HelicopterDetailsPanel::closeRequested);
    headerLayout->addWidget(btnClose);

    mainLayout->addLayout(headerLayout);

    // ==========================================
    // 2. Scrollable Telemetry Area
    // ==========================================
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; } QScrollBar:vertical { width: 4px; background: transparent; } QScrollBar::handle:vertical { background: #3F3F46; border-radius: 2px; }");

    auto* scrollContainer = new QWidget(scrollArea);
    auto* contentLayout = new QVBoxLayout(scrollContainer);
    contentLayout->setContentsMargins(0, 0, 4, 0);
    contentLayout->setSpacing(10);

    // Hero Identity Card
    auto* heroCard = new QFrame(scrollContainer);
    heroCard->setStyleSheet("background-color: #1C1C22; border: 1px solid #2E2E38; border-radius: 8px; padding: 10px;");
    auto* heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(10, 10, 10, 10);
    heroLayout->setSpacing(4);

    auto* heroTopRow = new QHBoxLayout();
    lblCallsign = new QLabel("SELECT HELICOPTER", heroCard);
    lblCallsign->setStyleSheet("font-size: 16px; font-weight: 800; color: #F4F4F5; letter-spacing: 0.5px;");
    heroTopRow->addWidget(lblCallsign);
    heroTopRow->addStretch(1);

    lblEmergencyBadge = new QLabel("ACTIVE", heroCard);
    lblEmergencyBadge->setStyleSheet("font-size: 9px; font-weight: 700; color: #06B6D4; background-color: rgba(6, 182, 212, 0.20); border: 1px solid #06B6D4; border-radius: 3px; padding: 2px 5px;");
    heroTopRow->addWidget(lblEmergencyBadge);
    heroLayout->addLayout(heroTopRow);

    lblModel = new QLabel("Click any helicopter dot on the map", heroCard);
    lblModel->setStyleSheet("font-size: 12px; font-weight: 600; color: #F59E0B;");
    lblModel->setWordWrap(true);
    heroLayout->addWidget(lblModel);

    lblOperator = new QLabel("Real-time telemetry stream from Airplanes.live / ADS-B", heroCard);
    lblOperator->setStyleSheet("font-size: 11px; color: #9CA3AF;");
    lblOperator->setWordWrap(true);
    heroLayout->addWidget(lblOperator);

    contentLayout->addWidget(heroCard);

    // Bento Telemetry Metrics
    contentLayout->addWidget(createMetricCard("✈️", "Barometric Altitude", lblAltitudeVal, "ft MSL"));
    contentLayout->addWidget(createMetricCard("💨", "Ground Speed", lblSpeedVal, "kt"));
    contentLayout->addWidget(createMetricCard("🧭", "Track / Heading", lblHeadingVal, "deg"));
    contentLayout->addWidget(createMetricCard("📈", "Vertical Climb/Descent", lblVerticalRateVal, "ft/min"));
    contentLayout->addWidget(createMetricCard("📍", "GPS Coordinates", lblCoordinatesVal, "lat / lon"));
    contentLayout->addWidget(createMetricCard("📡", "Transponder / Squawk", lblTransponderVal, "ICAO"));

    // Center Map Action Button
    btnCenterMap = new QPushButton("🎯 Fly & Track on Map", scrollContainer);
    btnCenterMap->setStyleSheet("QPushButton { background-color: #F59E0B; color: #09090B; border: none; border-radius: 6px; font-weight: 700; font-size: 11px; padding: 7px 12px; } QPushButton:hover { background-color: #FBBF24; }");
    connect(btnCenterMap, &QPushButton::clicked, this, &HelicopterDetailsPanel::onCenterClicked);
    contentLayout->addWidget(btnCenterMap);

    // ==========================================
    // 3. Active Regional Fleet Section
    // ==========================================
    auto* lblFleetTitle = new QLabel("🚁 Active Helicopter Fleet in Airspace", scrollContainer);
    lblFleetTitle->setStyleSheet("font-size: 11px; font-weight: 700; color: #D4D4D8; margin-top: 8px;");
    contentLayout->addWidget(lblFleetTitle);

    fleetListWidget = new QListWidget(scrollContainer);
    fleetListWidget->setStyleSheet("QListWidget { background-color: #18181E; border: 1px solid #27272A; border-radius: 6px; color: #E4E4E7; font-size: 11px; } QListWidget::item { padding: 6px; border-bottom: 1px solid #202026; } QListWidget::item:hover { background-color: #272730; color: #F59E0B; } QListWidget::item:selected { background-color: #2E2E3B; color: #FBBF24; font-weight: bold; }");
    fleetListWidget->setMinimumHeight(150);
    connect(fleetListWidget, &QListWidget::itemClicked, this, &HelicopterDetailsPanel::onListItemClicked);
    contentLayout->addWidget(fleetListWidget);

    contentLayout->addStretch(1);
    scrollArea->setWidget(scrollContainer);
    mainLayout->addWidget(scrollArea, 1);
}

QWidget* HelicopterDetailsPanel::createMetricCard(const QString& icon, const QString& title, QLabel*& valLabel, const QString& unit) {
    auto* card = new QFrame(this);
    card->setStyleSheet("background-color: #191920; border: 1px solid #272730; border-radius: 6px;");
    auto* layout = new QHBoxLayout(card);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(8);

    auto* lblIcon = new QLabel(icon, card);
    lblIcon->setStyleSheet("font-size: 14px;");
    layout->addWidget(lblIcon);

    auto* col = new QVBoxLayout();
    col->setContentsMargins(0, 0, 0, 0);
    col->setSpacing(1);

    auto* lblTitle = new QLabel(title, card);
    lblTitle->setStyleSheet("font-size: 9px; font-weight: 600; color: #71717A; text-transform: uppercase;");
    col->addWidget(lblTitle);

    valLabel = new QLabel("--", card);
    valLabel->setStyleSheet("font-size: 12px; font-weight: 700; color: #F4F4F5;");
    col->addWidget(valLabel);

    layout->addLayout(col, 1);

    auto* lblUnit = new QLabel(unit, card);
    lblUnit->setStyleSheet("font-size: 9px; color: #52525B; font-weight: 600;");
    layout->addWidget(lblUnit);

    return card;
}

void HelicopterDetailsPanel::setHelicopter(const MapCore::HelicopterTrack& heli) {
    currentHeli = heli;

    lblCallsign->setText(heli.flight.isEmpty() ? heli.registration : heli.flight);
    lblModel->setText(heli.modelName);
    lblOperator->setText(QString("%1 • %2").arg(heli.operatorName).arg(heli.source.isEmpty() ? "Airplanes.live / OpenSky" : heli.source));

    if (heli.emergency != "none" && !heli.emergency.isEmpty()) {
        lblEmergencyBadge->setText(QString("⚠️ %1").arg(heli.emergency.toUpper()));
        lblEmergencyBadge->setStyleSheet("font-size: 9px; font-weight: 700; color: #EF4444; background-color: rgba(239, 68, 68, 0.20); border: 1px solid #EF4444; border-radius: 3px; padding: 2px 5px;");
    } else {
        lblEmergencyBadge->setText("ACTIVE");
        lblEmergencyBadge->setStyleSheet("font-size: 9px; font-weight: 700; color: #10B981; background-color: rgba(16, 185, 129, 0.20); border: 1px solid #10B981; border-radius: 3px; padding: 2px 5px;");
    }

    if (lblAltitudeVal) {
        lblAltitudeVal->setText(QString("%1 ft (%2 m)")
                                   .arg(static_cast<int>(heli.altitudeFt))
                                   .arg(static_cast<int>(heli.altitudeMeters)));
    }
    if (lblSpeedVal) {
        lblSpeedVal->setText(QString("%1 kt (%2 km/h)")
                                .arg(static_cast<int>(heli.groundSpeedKnots))
                                .arg(static_cast<int>(heli.groundSpeedKmh)));
    }
    if (lblHeadingVal) {
        lblHeadingVal->setText(QString("%1°").arg(static_cast<int>(heli.trackHeading)));
    }
    if (lblVerticalRateVal) {
        QString sign = heli.verticalRateFpm > 0 ? "+" : "";
        lblVerticalRateVal->setText(QString("%1%2 ft/min").arg(sign).arg(static_cast<int>(heli.verticalRateFpm)));
        if (heli.verticalRateFpm > 100) {
            lblVerticalRateVal->setStyleSheet("font-size: 12px; font-weight: 700; color: #10B981;");
        } else if (heli.verticalRateFpm < -100) {
            lblVerticalRateVal->setStyleSheet("font-size: 12px; font-weight: 700; color: #F59E0B;");
        } else {
            lblVerticalRateVal->setStyleSheet("font-size: 12px; font-weight: 700; color: #F4F4F5;");
        }
    }
    if (lblCoordinatesVal) {
        lblCoordinatesVal->setText(QString("%1° N, %2° E")
                                      .arg(heli.lat, 0, 'f', 4)
                                      .arg(heli.lon, 0, 'f', 4));
    }
    if (lblTransponderVal) {
        lblTransponderVal->setText(QString("Hex: %1 · Sq: %2")
                                      .arg(heli.hex)
                                      .arg(heli.squawk));
    }

    // Highlight item in list widget
    for (int i = 0; i < fleetListWidget->count(); ++i) {
        auto* item = fleetListWidget->item(i);
        if (item && item->data(Qt::UserRole).toString() == heli.hex) {
            fleetListWidget->setCurrentItem(item);
            break;
        }
    }
}

void HelicopterDetailsPanel::updateHelicopterList(const std::vector<MapCore::HelicopterTrack>& list) {
    currentFleet = list;
    QString prevSelectedHex = currentHeli.hex;

    fleetListWidget->clear();
    for (const auto& h : list) {
        QString text = QString("🚁 %1 · %2 (%3 ft · %4 kt)")
                           .arg(h.flight.isEmpty() ? h.registration : h.flight)
                           .arg(h.typeCode)
                           .arg(static_cast<int>(h.altitudeFt))
                           .arg(static_cast<int>(h.groundSpeedKnots));

        auto* item = new QListWidgetItem(text, fleetListWidget);
        item->setData(Qt::UserRole, h.hex);
        if (h.hex == prevSelectedHex) {
            fleetListWidget->setCurrentItem(item);
        }
    }

    if (!list.empty() && currentHeli.hex.isEmpty()) {
        setHelicopter(list.front());
    } else if (!currentHeli.hex.isEmpty()) {
        for (const auto& h : list) {
            if (h.hex == currentHeli.hex) {
                setHelicopter(h);
                break;
            }
        }
    }
}

void HelicopterDetailsPanel::setLiveStatus(const QString& statusText) {
    if (lblHeaderStatus) {
        lblHeaderStatus->setText(statusText);
    }
}

void HelicopterDetailsPanel::onListItemClicked(QListWidgetItem* item) {
    if (!item) return;
    QString hex = item->data(Qt::UserRole).toString();
    emit helicopterSelectedFromList(hex);
}

void HelicopterDetailsPanel::onCenterClicked() {
    if (std::abs(currentHeli.lat) > 0.001 || std::abs(currentHeli.lon) > 0.001) {
        emit centerMapRequested(currentHeli.lat, currentHeli.lon);
    }
}

} // namespace MapUI
