#include "PlaceCard.h"
#include <QGraphicsDropShadowEffect>
#include <QHeaderView>
#include <QClipboard>
#include <QApplication>
#include <cmath>

namespace MapUI {

PlaceCard::PlaceCard(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void PlaceCard::setupUi() {
    setFixedWidth(360);
    setStyleSheet(R"(
        QWidget#placeCardContainer {
            background-color: #202124;
            border: 1px solid #3C4043;
            border-radius: 12px;
        }
    )");

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 100));
    shadow->setOffset(0, 5);
    setGraphicsEffect(shadow);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);

    auto* container = new QWidget(this);
    container->setObjectName("placeCardContainer");
    rootLayout->addWidget(container);

    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    // Header row (Icon + Title + Close)
    auto* headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(8);

    lblIcon = new QLabel("📍", container);
    lblIcon->setStyleSheet("font-size: 24px;");

    lblTitle = new QLabel("Location Details", container);
    lblTitle->setStyleSheet("font-family: 'Segoe UI', Arial, sans-serif; font-size: 16px; font-weight: bold; color: #E8EAED;");
    lblTitle->setWordWrap(true);

    btnClose = new QPushButton("✕", container);
    btnClose->setFlat(true);
    btnClose->setFixedSize(28, 28);
    btnClose->setStyleSheet(R"(
        QPushButton {
            border: none;
            border-radius: 14px;
            font-size: 13px;
            color: #9AA0A6;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #303134;
            color: #E8EAED;
        }
    )");

    headerLayout->addWidget(lblIcon);
    headerLayout->addWidget(lblTitle, 1);
    headerLayout->addWidget(btnClose);

    // Category badge & distance
    auto* metaLayout = new QHBoxLayout();
    lblCategory = new QLabel("Category", container);
    lblCategory->setStyleSheet(R"(
        background-color: #38465C;
        color: #8AB4F8;
        font-family: 'Segoe UI', Arial, sans-serif;
        font-size: 11px;
        font-weight: bold;
        padding: 3px 8px;
        border-radius: 10px;
    )");

    lblDistance = new QLabel("", container);
    lblDistance->setStyleSheet("font-family: 'Segoe UI', Arial, sans-serif; font-size: 11px; color: #9AA0A6;");

    metaLayout->addWidget(lblCategory);
    metaLayout->addSpacing(6);
    metaLayout->addWidget(lblDistance);
    metaLayout->addStretch();

    // Coordinates row
    auto* coordsLayout = new QHBoxLayout();
    lblCoords = new QLabel("26.18° N, 91.74° E", container);
    lblCoords->setStyleSheet("font-family: 'Segoe UI', Arial, sans-serif; font-size: 12px; color: #BDC1C6;");

    btnCopyCoords = new QPushButton("📋 Copy", container);
    btnCopyCoords->setStyleSheet(R"(
        QPushButton {
            background-color: #303134;
            border: 1px solid #3C4043;
            border-radius: 12px;
            font-size: 11px;
            color: #E8EAED;
            padding: 2px 8px;
        }
        QPushButton:hover {
            background-color: #3C4043;
            color: #8AB4F8;
            border-color: #8AB4F8;
        }
    )");

    coordsLayout->addWidget(lblCoords, 1);
    coordsLayout->addWidget(btnCopyCoords);

    // Tags Table (Dark Theme)
    tableTags = new QTableWidget(container);
    tableTags->setColumnCount(2);
    tableTags->setHorizontalHeaderLabels({ "Attribute", "Value" });
    tableTags->horizontalHeader()->setStretchLastSection(true);
    tableTags->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tableTags->verticalHeader()->setVisible(false);
    tableTags->setSelectionMode(QAbstractItemView::NoSelection);
    tableTags->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableTags->setStyleSheet(R"(
        QTableWidget {
            border: 1px solid #3C4043;
            border-radius: 6px;
            font-size: 11px;
            background-color: #1A1D21;
            color: #E8EAED;
            gridline-color: #303134;
        }
        QHeaderView::section {
            background-color: #303134;
            color: #BDC1C6;
            font-weight: bold;
            border: none;
            padding: 4px;
        }
    )");
    tableTags->setMaximumHeight(130);

    // Action Buttons
    auto* actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(8);

    btnZoomIn = new QPushButton("🔍 Zoom In", container);
    btnZoomIn->setStyleSheet(R"(
        QPushButton {
            background-color: #8AB4F8;
            color: #202124;
            border: none;
            border-radius: 16px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 12px;
            font-weight: bold;
            padding: 6px 14px;
        }
        QPushButton:hover {
            background-color: #AECBFA;
        }
        QPushButton:pressed {
            background-color: #669DF6;
        }
    )");

    btnMeasure = new QPushButton("📏 Measure", container);
    btnMeasure->setStyleSheet(R"(
        QPushButton {
            background-color: #303134;
            color: #8AB4F8;
            border: 1px solid #3C4043;
            border-radius: 16px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 12px;
            font-weight: bold;
            padding: 6px 14px;
        }
        QPushButton:hover {
            background-color: #3C4043;
            border-color: #8AB4F8;
        }
    )");

    actionLayout->addWidget(btnZoomIn);
    actionLayout->addWidget(btnMeasure);

    layout->addLayout(headerLayout);
    layout->addLayout(metaLayout);
    layout->addLayout(coordsLayout);
    layout->addWidget(tableTags);
    layout->addLayout(actionLayout);

    connect(btnClose, &QPushButton::clicked, this, [this]() {
        hide();
        emit cardClosed();
    });

    connect(btnZoomIn, &QPushButton::clicked, this, [this]() {
        emit zoomInRequested(currentFeature.mercatorPos);
    });

    connect(btnMeasure, &QPushButton::clicked, this, [this]() {
        emit measureFromRequested(currentFeature.mercatorPos);
    });

    connect(btnCopyCoords, &QPushButton::clicked, this, [this]() {
        QString text = QString("%1, %2").arg(currentFeature.geoCoord.lat, 0, 'f', 6)
                                        .arg(currentFeature.geoCoord.lon, 0, 'f', 6);
        QApplication::clipboard()->setText(text);
        btnCopyCoords->setText("✓ Copied");
    });
}

QString PlaceCard::formatDMS(double val, bool isLat) const {
    char dir = isLat ? (val >= 0 ? 'N' : 'S') : (val >= 0 ? 'E' : 'W');
    double absVal = std::abs(val);
    int deg = static_cast<int>(absVal);
    double minDouble = (absVal - deg) * 60.0;
    int min = static_cast<int>(minDouble);
    double sec = (minDouble - min) * 60.0;
    return QString("%1° %2' %3\" %4").arg(deg).arg(min).arg(sec, 0, 'f', 1).arg(dir);
}

void PlaceCard::setFeature(const MapCore::FeatureInfo& info) {
    currentFeature = info;
    btnCopyCoords->setText("📋 Copy");

    lblIcon->setText(QString::fromUtf8(MapCore::getCategoryIconEmoji(info.category)));
    lblTitle->setText(info.name.empty() ? QString::fromUtf8(MapCore::getCategoryDisplayName(info.category))
                                        : QString::fromStdString(info.name));
    lblCategory->setText(QString::fromUtf8(MapCore::getCategoryDisplayName(info.category)));

    if (info.distanceMeters > 0.0f) {
        if (info.distanceMeters < 1000.0f) {
            lblDistance->setText(QString("• %1 m away").arg(qRound(info.distanceMeters)));
        } else {
            lblDistance->setText(QString("• %1 km away").arg(info.distanceMeters / 1000.0f, 0, 'f', 2));
        }
    } else {
        lblDistance->setText("");
    }

    QString dmsStr = QString("%1, %2").arg(formatDMS(info.geoCoord.lat, true))
                                      .arg(formatDMS(info.geoCoord.lon, false));
    lblCoords->setText(dmsStr);

    // Populate tags
    tableTags->setRowCount(static_cast<int>(info.tags.size()));
    for (int i = 0; i < (int)info.tags.size(); ++i) {
        tableTags->setItem(i, 0, new QTableWidgetItem(QString::fromStdString(info.tags[i].first)));
        tableTags->setItem(i, 1, new QTableWidgetItem(QString::fromStdString(info.tags[i].second)));
    }

    tableTags->setVisible(!info.tags.empty());
    show();
}

} // namespace MapUI
