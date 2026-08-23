#include "MapRenderer.h"
#include <QPolygonF>
#include <QPainterPath>
#include <QFontMetricsF>
#include <algorithm>
#include <cmath>

namespace MapRenderer {

void MapRenderer::render(QPainter& painter, const MapCore::SpatialIndex& index,
                         const MapCore::Point2D& center, float zoomLevel,
                         int width, int height,
                         const MapCore::Point2D& selectedPos,
                         const std::vector<MapCore::Point2D>& measurePoints) {
    if (width <= 0 || height <= 0) return;

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    QRect viewportRect(0, 0, width, height);

    // 1. Fill background (ocean / default land color)
    painter.fillRect(viewportRect, style.backgroundColor);

    // 2. Query visible features from spatial index
    MapCore::BoundingBox viewBbox = getViewportMercatorBbox(center, zoomLevel, width, height);

    std::vector<const MapCore::MapPolygon*> visiblePolygons;
    std::vector<const MapCore::MapPolyline*> visiblePolylines;
    std::vector<const MapCore::MapPoint*> visiblePoints;

    index.queryViewport(viewBbox, zoomLevel, visiblePolygons, visiblePolylines, visiblePoints);

    double scale = calculateScale(zoomLevel);
    double cx = center.x;
    double cy = center.y;
    double halfW = width * 0.5;
    double halfH = height * 0.5;

    auto toScreen = [&](const MapCore::Point2D& pt) -> QPointF {
        return QPointF((pt.x - cx) * scale + halfW, (pt.y - cy) * scale + halfH);
    };

    // Helper for road width scaling per zoom
    float widthFactor = std::pow(1.15f, std::clamp(zoomLevel - 11.0f, -4.0f, 7.0f));

    // 3. Render Landuse & Natural Polygons (Forests, Parks, Farmland, Residential)
    if (options.showLanduse) {
        for (const auto* poly : visiblePolygons) {
            if (poly->category == MapCore::FeatureCategory::WATER_LAKE ||
                poly->category == MapCore::FeatureCategory::WATER_OCEAN ||
                poly->category == MapCore::FeatureCategory::WATER_RIVER ||
                poly->category == MapCore::FeatureCategory::BUILDING ||
                poly->category == MapCore::FeatureCategory::AEROWAY_RUNWAY) {
                continue;
            }

            const auto& pStyle = style.getPolygonStyle(poly->category);
            QPolygonF screenPoly;
            screenPoly.reserve(poly->points.size());
            for (const auto& pt : poly->points) {
                screenPoly.append(toScreen(pt));
            }

            painter.setBrush(pStyle.fillColor);
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(screenPoly);
        }
    }

    // 4. Render Water Polygons (Lakes, Reservoirs, Wide Rivers)
    if (options.showWater) {
        for (const auto* poly : visiblePolygons) {
            if (poly->category != MapCore::FeatureCategory::WATER_LAKE &&
                poly->category != MapCore::FeatureCategory::WATER_OCEAN &&
                poly->category != MapCore::FeatureCategory::WATER_RIVER) {
                continue;
            }

            const auto& pStyle = style.getPolygonStyle(poly->category);
            QPolygonF screenPoly;
            screenPoly.reserve(poly->points.size());
            for (const auto& pt : poly->points) {
                screenPoly.append(toScreen(pt));
            }

            painter.setBrush(pStyle.fillColor);
            if (pStyle.hasOutline) {
                painter.setPen(QPen(pStyle.outlineColor, pStyle.outlineWidth));
            } else {
                painter.setPen(Qt::NoPen);
            }
            painter.drawPolygon(screenPoly);
        }
    }

    // 5. Render Aeroways (Runways & Taxiways)
    for (const auto* poly : visiblePolygons) {
        if (poly->category != MapCore::FeatureCategory::AEROWAY_RUNWAY) continue;
        const auto& pStyle = style.getPolygonStyle(poly->category);
        QPolygonF screenPoly;
        screenPoly.reserve(poly->points.size());
        for (const auto& pt : poly->points) {
            screenPoly.append(toScreen(pt));
        }
        painter.setBrush(pStyle.fillColor);
        painter.setPen(QPen(pStyle.outlineColor, 1.0f));
        painter.drawPolygon(screenPoly);
    }

    // 6. Render Buildings (Zoom >= 14) with subtle shadow
    if (options.showBuildings && zoomLevel >= 14.0f) {
        const auto& bStyle = style.getPolygonStyle(MapCore::FeatureCategory::BUILDING);
        for (const auto* poly : visiblePolygons) {
            if (poly->category != MapCore::FeatureCategory::BUILDING) continue;

            QPolygonF screenPoly;
            screenPoly.reserve(poly->points.size());
            for (const auto& pt : poly->points) {
                screenPoly.append(toScreen(pt));
            }

            // Draw subtle drop shadow
            if (zoomLevel >= 15.5f) {
                painter.setBrush(style.buildingShadow);
                painter.setPen(Qt::NoPen);
                painter.drawPolygon(screenPoly.translated(1.5, 1.5));
            }

            painter.setBrush(bStyle.fillColor);
            painter.setPen(QPen(bStyle.outlineColor, bStyle.outlineWidth));
            painter.drawPolygon(screenPoly);
        }
    }

    // 7. Render Polylines - Pass 1: River and Stream Lines
    if (options.showWater) {
        for (const auto* line : visiblePolylines) {
            if (line->category != MapCore::FeatureCategory::WATER_RIVER &&
                line->category != MapCore::FeatureCategory::WATER_STREAM &&
                line->category != MapCore::FeatureCategory::WATER_CANAL) {
                continue;
            }

            const auto& rStyle = style.getRoadStyle(line->category);
            float w = rStyle.baseWidth * widthFactor;
            if (line->category == MapCore::FeatureCategory::WATER_RIVER) w = std::max(w, 2.5f);

            const auto& pts = (zoomLevel <= 11.0f && !line->lodPoints.empty()) ? line->lodPoints : line->points;
            if (pts.size() < 2) continue;

            QPainterPath path;
            path.moveTo(toScreen(pts[0]));
            for (size_t i = 1; i < pts.size(); ++i) {
                path.lineTo(toScreen(pts[i]));
            }

            painter.setPen(QPen(rStyle.coreColor, w, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }
    }

    // 8. Render Roads - Pass 2: Road Casings (Underlayer)
    if (options.showRoads) {
        for (const auto* line : visiblePolylines) {
            if (line->category == MapCore::FeatureCategory::WATER_RIVER ||
                line->category == MapCore::FeatureCategory::WATER_STREAM ||
                line->category == MapCore::FeatureCategory::WATER_CANAL ||
                line->category == MapCore::FeatureCategory::BOUNDARY_STATE) {
                continue;
            }

            const auto& rStyle = style.getRoadStyle(line->category);
            float coreW = rStyle.baseWidth * widthFactor;
            float totalW = coreW + rStyle.casingExtra * (zoomLevel >= 12.0f ? 2.0f : 1.2f);

            const auto& pts = (zoomLevel <= 11.0f && !line->lodPoints.empty()) ? line->lodPoints : line->points;
            if (pts.size() < 2) continue;

            QPainterPath path;
            path.moveTo(toScreen(pts[0]));
            for (size_t i = 1; i < pts.size(); ++i) {
                path.lineTo(toScreen(pts[i]));
            }

            painter.setPen(QPen(rStyle.casingColor, totalW, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }

        // 9. Render Roads - Pass 3: Road Cores / Fills (Overlayer)
        for (const auto* line : visiblePolylines) {
            if (line->category == MapCore::FeatureCategory::WATER_RIVER ||
                line->category == MapCore::FeatureCategory::WATER_STREAM ||
                line->category == MapCore::FeatureCategory::WATER_CANAL ||
                line->category == MapCore::FeatureCategory::BOUNDARY_STATE) {
                continue;
            }

            const auto& rStyle = style.getRoadStyle(line->category);
            float coreW = rStyle.baseWidth * widthFactor;

            const auto& pts = (zoomLevel <= 11.0f && !line->lodPoints.empty()) ? line->lodPoints : line->points;
            if (pts.size() < 2) continue;

            QPainterPath path;
            path.moveTo(toScreen(pts[0]));
            for (size_t i = 1; i < pts.size(); ++i) {
                path.lineTo(toScreen(pts[i]));
            }

            painter.setPen(QPen(rStyle.coreColor, coreW, rStyle.penStyle, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(path);
        }
    }

    // 10. Render Boundaries (Dashed Lines)
    for (const auto* line : visiblePolylines) {
        if (line->category != MapCore::FeatureCategory::BOUNDARY_STATE &&
            line->category != MapCore::FeatureCategory::BOUNDARY_DISTRICT) {
            continue;
        }

        const auto& rStyle = style.getRoadStyle(line->category);
        const auto& pts = line->points;
        if (pts.size() < 2) continue;

        QPainterPath path;
        path.moveTo(toScreen(pts[0]));
        for (size_t i = 1; i < pts.size(); ++i) {
            path.lineTo(toScreen(pts[i]));
        }

        painter.setPen(QPen(rStyle.casingColor, 2.0f, Qt::DashLine, Qt::FlatCap, Qt::MiterJoin));
        painter.drawPath(path);
    }

    // 11. Place Labels & POIs with Collision Avoidance
    if (options.showLabels) {
        labelPlacer.beginFrame(QRectF(0, 0, width, height));

        // Sort visible points by priority (1 = highest priority)
        std::vector<const MapCore::MapPoint*> sortedPoints = visiblePoints;
        std::sort(sortedPoints.begin(), sortedPoints.end(), [](const MapCore::MapPoint* a, const MapCore::MapPoint* b) {
            return a->priority < b->priority;
        });

        QFontMetricsF fmCity(style.labelCity.font);
        QFontMetricsF fmTown(style.labelTown.font);
        QFontMetricsF fmVillage(style.labelVillage.font);
        QFontMetricsF fmPoi(style.labelPoi.font);

        for (const auto* pt : sortedPoints) {
            if (!options.showPois && pt->priority >= 3 && pt->category != MapCore::FeatureCategory::PLACE_CITY && pt->category != MapCore::FeatureCategory::PLACE_TOWN) {
                continue;
            }

            QPointF sPos = toScreen(pt->pos);
            const auto& tStyle = style.getTextStyle(pt->category);
            QString nameStr = QString::fromStdString(pt->name);
            QString emoji = QString::fromUtf8(MapCore::getCategoryIconEmoji(pt->category));

            QFontMetricsF* fm = &fmPoi;
            if (pt->category == MapCore::FeatureCategory::PLACE_CITY) fm = &fmCity;
            else if (pt->category == MapCore::FeatureCategory::PLACE_TOWN) fm = &fmTown;
            else if (pt->category == MapCore::FeatureCategory::PLACE_VILLAGE) fm = &fmVillage;

            labelPlacer.tryPlaceLabel(nameStr, sPos, pt->category, tStyle, pt->priority, *fm, emoji);
        }

        labelPlacer.drawAll(painter);
    }

    // 12. Render Distance Measurement Path & Waypoints
    if (!measurePoints.empty()) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        if (measurePoints.size() >= 2) {
            QPainterPath mPath;
            mPath.moveTo(toScreen(measurePoints[0]));
            for (size_t i = 1; i < measurePoints.size(); ++i) {
                mPath.lineTo(toScreen(measurePoints[i]));
            }

            // Outline
            painter.setPen(QPen(Qt::white, 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(mPath);

            // Core line
            painter.setPen(QPen(style.measureLineColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.drawPath(mPath);
        }

        // Draw Waypoint pins & distances
        double totalDistM = 0.0;
        for (size_t i = 0; i < measurePoints.size(); ++i) {
            QPointF ptScreen = toScreen(measurePoints[i]);

            if (i > 0) {
                MapCore::GeoCoord c1 = MapCore::Projection::mercatorToGeo(measurePoints[i-1]);
                MapCore::GeoCoord c2 = MapCore::Projection::mercatorToGeo(measurePoints[i]);
                totalDistM += MapCore::Projection::haversineDistanceMeters(c1, c2);
            }

            // Pin dot
            painter.setBrush(style.measureLineColor);
            painter.setPen(QPen(Qt::white, 2.5));
            painter.drawEllipse(ptScreen, 6.0, 6.0);

            // Distance label badge
            QString distText = (totalDistM < 1000.0)
                ? QString("%1 m").arg(qRound(totalDistM))
                : QString("%1 km").arg(totalDistM / 1000.0, 0, 'f', 2);

            QFont badgeFont("Segoe UI", 9, QFont::Bold);
            painter.setFont(badgeFont);
            QFontMetricsF bfm(badgeFont);
            QRectF badgeRect(ptScreen.x() + 10, ptScreen.y() - 12, bfm.horizontalAdvance(distText) + 12, bfm.height() + 4);

            painter.setBrush(QColor(32, 33, 36, 220));
            painter.setPen(QPen(Qt::white, 1.0));
            painter.drawRoundedRect(badgeRect, 4, 4);

            painter.setPen(Qt::white);
            painter.drawText(badgeRect, Qt::AlignCenter, distText);
        }
        painter.restore();
    }

    // 13. Render Selected Feature Pin / Marker (Google Maps Red Pin)
    if (selectedPos.x >= 0.0f && selectedPos.y >= 0.0f) {
        QPointF pinPos = toScreen(selectedPos);
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);

        // Pulsing radar ring
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(style.selectionRingColor, 3.0));
        painter.drawEllipse(pinPos, 18.0, 18.0);
        painter.setPen(QPen(style.selectionRingColor, 1.5, Qt::DashLine));
        painter.drawEllipse(pinPos, 28.0, 28.0);

        // Pin shadow
        painter.setBrush(QColor(0, 0, 0, 80));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(pinPos + QPointF(2, 2), 6.0, 3.0);

        // Red Pin Path
        QPainterPath pinPath;
        qreal px = pinPos.x();
        qreal py = pinPos.y();

        pinPath.moveTo(px, py);
        pinPath.cubicTo(px - 10, py - 16, px - 12, py - 26, px, py - 26);
        pinPath.cubicTo(px + 12, py - 26, px + 10, py - 16, px, py);

        painter.setBrush(style.selectionPinColor);
        painter.setPen(QPen(Qt::white, 1.5));
        painter.drawPath(pinPath);

        // Inner white dot
        painter.setBrush(Qt::white);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QPointF(px, py - 20), 4.0, 4.0);

        painter.restore();
    }
}

} // namespace MapRenderer
