#pragma once

#include <QWidget>
#include <QPainter>
#include <QPolygonF>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <vector>
#include "../core/SpatialIndex.h"
#include "../core/GeoTypes.h"

namespace MapUI {

enum class MiniMapMode {
    Online_India = 0,
    Offline_Assam = 1
};

class MiniMap : public QWidget {
    Q_OBJECT

private:
    const MapCore::SpatialIndex* spatialIndex = nullptr;
    MapCore::BoundingBox currentViewBbox;
    MapCore::BoundingBox fullAssamExtent;
    MapCore::BoundingBox fullIndiaExtent;

    MiniMapMode currentMode = MiniMapMode::Online_India;

    // Vector polygon coordinates for India outline (normalized Mercator)
    std::vector<MapCore::Point2D> indiaBorder = {
        MapCore::Point2D(0.70778f, 0.39440f),
        MapCore::Point2D(0.71250f, 0.39269f),
        MapCore::Point2D(0.71806f, 0.39779f),
        MapCore::Point2D(0.71889f, 0.40445f),
        MapCore::Point2D(0.72500f, 0.41097f),
        MapCore::Point2D(0.73056f, 0.41735f),
        MapCore::Point2D(0.74583f, 0.42050f),
        MapCore::Point2D(0.75556f, 0.41893f),
        MapCore::Point2D(0.76667f, 0.41735f),
        MapCore::Point2D(0.76944f, 0.42050f),
        MapCore::Point2D(0.76250f, 0.42671f),
        MapCore::Point2D(0.75972f, 0.43281f),
        MapCore::Point2D(0.75778f, 0.43733f),
        MapCore::Point2D(0.74861f, 0.43583f),
        MapCore::Point2D(0.74306f, 0.43793f),
        MapCore::Point2D(0.73889f, 0.44328f),
        MapCore::Point2D(0.73056f, 0.45061f),
        MapCore::Point2D(0.72278f, 0.46072f),
        MapCore::Point2D(0.72167f, 0.47067f),
        MapCore::Point2D(0.71528f, 0.47742f),
        MapCore::Point2D(0.71250f, 0.47546f),
        MapCore::Point2D(0.70833f, 0.46642f),
        MapCore::Point2D(0.70500f, 0.45785f),
        MapCore::Point2D(0.70222f, 0.44623f),
        MapCore::Point2D(0.70222f, 0.44180f),
        MapCore::Point2D(0.69444f, 0.44031f),
        MapCore::Point2D(0.69167f, 0.43583f),
        MapCore::Point2D(0.69028f, 0.43190f),
        MapCore::Point2D(0.69722f, 0.42977f),
        MapCore::Point2D(0.69583f, 0.42050f),
        MapCore::Point2D(0.70139f, 0.41258f),
        MapCore::Point2D(0.70694f, 0.40445f),
        MapCore::Point2D(0.70556f, 0.39779f),
        MapCore::Point2D(0.70778f, 0.39440f)
    };

public:
    explicit MiniMap(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(180, 130);
        fullAssamExtent = MapCore::BoundingBox(0.748f, 0.420f, 0.768f, 0.440f);
        fullIndiaExtent = MapCore::BoundingBox(0.685f, 0.380f, 0.775f, 0.485f);

        auto* shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(16);
        shadow->setColor(QColor(0, 0, 0, 120));
        shadow->setOffset(0, 4);
        setGraphicsEffect(shadow);
    }

    void setMode(MiniMapMode mode) {
        currentMode = mode;
        update();
    }

    MiniMapMode getMode() const {
        return currentMode;
    }

    void setSpatialIndex(const MapCore::SpatialIndex* index) {
        spatialIndex = index;
        if (index && index->extent.isValid()) {
            fullAssamExtent = index->extent;
        }
        update();
    }

    void setViewport(const MapCore::BoundingBox& bbox) {
        currentViewBbox = bbox;
        update();
    }

    void setOnlineViewport(double lat, double lon, int zoom, int viewWidth, int viewHeight) {
        // Compute normalized Mercator bounding box for current online view
        MapCore::Point2D centerPt = MapCore::Projection::geoToMercator(MapCore::GeoCoord(lat, lon));
        double tileCount = std::pow(2.0, zoom);
        double spanX = static_cast<double>(viewWidth) / (256.0 * tileCount);
        double spanY = static_cast<double>(viewHeight) / (256.0 * tileCount);

        currentViewBbox = MapCore::BoundingBox(
            static_cast<float>(centerPt.x - spanX * 0.5),
            static_cast<float>(centerPt.y - spanY * 0.5),
            static_cast<float>(centerPt.x + spanX * 0.5),
            static_cast<float>(centerPt.y + spanY * 0.5)
        );
        update();
    }

signals:
    void centerRequested(MapCore::Point2D pos);
    void onlineCenterRequested(double lat, double lon);

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;

        float u = static_cast<float>(event->pos().x()) / width();
        float v = static_cast<float>(event->pos().y()) / height();

        if (currentMode == MiniMapMode::Online_India) {
            float mx = fullIndiaExtent.minX + u * fullIndiaExtent.width();
            float my = fullIndiaExtent.minY + v * fullIndiaExtent.height();
            MapCore::GeoCoord geo = MapCore::Projection::mercatorToGeo(MapCore::Point2D(mx, my));
            emit onlineCenterRequested(geo.lat, geo.lon);
        } else {
            if (fullAssamExtent.isValid()) {
                float mx = fullAssamExtent.minX + u * fullAssamExtent.width();
                float my = fullAssamExtent.minY + v * fullAssamExtent.height();
                emit centerRequested(MapCore::Point2D(mx, my));
            }
        }
    }

    void paintEvent(QPaintEvent* /*event*/) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        // Dark Card Background & Border
        painter.setBrush(QColor(18, 18, 20, 240));
        painter.setPen(QPen(QColor(60, 64, 67), 1.5));
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 8, 8);

        if (currentMode == MiniMapMode::Online_India) {
            paintIndiaOverview(painter);
        } else {
            paintAssamOverview(painter);
        }

        // Viewport rectangle indicator
        if (currentViewBbox.isValid()) {
            const auto& extent = (currentMode == MiniMapMode::Online_India) ? fullIndiaExtent : fullAssamExtent;
            auto toMini = [&](const MapCore::Point2D& pt) -> QPointF {
                float u = (pt.x - extent.minX) / extent.width();
                float v = (pt.y - extent.minY) / extent.height();
                return QPointF(u * width(), v * height());
            };

            QPointF p1 = toMini(MapCore::Point2D(currentViewBbox.minX, currentViewBbox.minY));
            QPointF p2 = toMini(MapCore::Point2D(currentViewBbox.maxX, currentViewBbox.maxY));
            QRectF vRect(p1, p2);

            // Clamp / draw rectangle with bright glowing indicator
            painter.setBrush(QColor(242, 139, 130, 45));
            painter.setPen(QPen(QColor(242, 139, 130), 1.8));
            painter.drawRect(vRect);

            // Center red dot in viewport
            QPointF cPos = (p1 + p2) * 0.5;
            if (rect().contains(cPos.toPoint())) {
                painter.setBrush(QColor(242, 139, 130));
                painter.setPen(QPen(Qt::white, 1.0));
                painter.drawEllipse(cPos, 3.0, 3.0);
            }
        }
    }

private:
    void paintIndiaOverview(QPainter& painter) {
        auto toMini = [&](const MapCore::Point2D& pt) -> QPointF {
            float u = (pt.x - fullIndiaExtent.minX) / fullIndiaExtent.width();
            float v = (pt.y - fullIndiaExtent.minY) / fullIndiaExtent.height();
            return QPointF(u * width(), v * height());
        };

        // 1. Draw India landmass polygon
        QPolygonF indiaPoly;
        indiaPoly.reserve(indiaBorder.size());
        for (const auto& pt : indiaBorder) {
            indiaPoly.append(toMini(pt));
        }

        painter.setBrush(QColor(30, 41, 59)); // Slate-800
        painter.setPen(QPen(QColor(56, 189, 248, 180), 1.5)); // Cyan-400 border
        painter.drawPolygon(indiaPoly);

        // 2. Highlight Assam region in emerald green
        MapCore::BoundingBox assamBbox(0.748f, 0.420f, 0.768f, 0.440f);
        QPointF a1 = toMini(MapCore::Point2D(assamBbox.minX, assamBbox.minY));
        QPointF a2 = toMini(MapCore::Point2D(assamBbox.maxX, assamBbox.maxY));
        QRectF assamRect(a1, a2);

        painter.setBrush(QColor(16, 185, 129, 120)); // Emerald fill
        painter.setPen(QPen(QColor(16, 185, 129), 1.2));
        painter.drawRoundedRect(assamRect, 3, 3);

        // 3. Mark Capital New Delhi & Guwahati
        MapCore::Point2D delhiPt = MapCore::Projection::geoToMercator(MapCore::GeoCoord(28.6139, 77.2090));
        QPointF delhiScreen = toMini(delhiPt);
        painter.setBrush(QColor(250, 204, 21)); // Gold dot
        painter.setPen(QPen(Qt::black, 1.0));
        painter.drawEllipse(delhiScreen, 3.5, 3.5);

        // Title Badge
        painter.setBrush(QColor(15, 23, 42, 210));
        painter.setPen(QPen(QColor(51, 65, 85), 1.0));
        painter.drawRoundedRect(6, height() - 22, 110, 16, 4, 4);

        painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
        painter.setPen(QColor(226, 232, 240));
        painter.drawText(10, height() - 10, "🇮🇳 India Overview");
    }

    void paintAssamOverview(QPainter& painter) {
        if (!fullAssamExtent.isValid()) return;

        auto toMini = [&](const MapCore::Point2D& pt) -> QPointF {
            float u = (pt.x - fullAssamExtent.minX) / fullAssamExtent.width();
            float v = (pt.y - fullAssamExtent.minY) / fullAssamExtent.height();
            return QPointF(u * width(), v * height());
        };

        // Draw simplified rivers in dark blue
        if (spatialIndex) {
            painter.setPen(QPen(QColor(56, 189, 248), 1.8));
            for (const auto& line : spatialIndex->polylines) {
                if (line.category == MapCore::FeatureCategory::WATER_RIVER) {
                    const auto& pts = !line.lodPoints.empty() ? line.lodPoints : line.points;
                    if (pts.size() < 2) continue;
                    for (size_t i = 1; i < pts.size(); ++i) {
                        painter.drawLine(toMini(pts[i-1]), toMini(pts[i]));
                    }
                }
            }

            // Draw major highways in glowing amber
            painter.setPen(QPen(QColor(251, 146, 60), 1.3));
            for (const auto& line : spatialIndex->polylines) {
                if (line.category == MapCore::FeatureCategory::HIGHWAY_MOTORWAY ||
                    line.category == MapCore::FeatureCategory::HIGHWAY_TRUNK) {
                    const auto& pts = !line.lodPoints.empty() ? line.lodPoints : line.points;
                    if (pts.size() < 2) continue;
                    for (size_t i = 1; i < pts.size(); ++i) {
                        painter.drawLine(toMini(pts[i-1]), toMini(pts[i]));
                    }
                }
            }
        }

        // Title Badge
        painter.setBrush(QColor(15, 23, 42, 210));
        painter.setPen(QPen(QColor(51, 65, 85), 1.0));
        painter.drawRoundedRect(6, height() - 22, 115, 16, 4, 4);

        painter.setFont(QFont("Segoe UI", 8, QFont::Bold));
        painter.setPen(QColor(226, 232, 240));
        painter.drawText(10, height() - 10, "🗺️ Assam Overview");
    }
};

} // namespace MapUI
