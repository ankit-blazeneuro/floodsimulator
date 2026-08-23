#pragma once

#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QPointF>
#include <QString>
#include <vector>
#include "MapStyle.h"
#include "../core/MapFeature.h"

namespace MapRenderer {

struct PlacedLabel {
    QString text;
    QString iconEmoji;
    QPointF screenPos;
    QRectF screenRect;
    MapCore::FeatureCategory category;
    TextStyle style;
    int priority = 5;
};

class LabelPlacer {
private:
    std::vector<PlacedLabel> placedLabels;
    QRectF viewportRect;

public:
    LabelPlacer() = default;

    void beginFrame(const QRectF& viewport) {
        placedLabels.clear();
        viewportRect = viewport;
    }

    bool tryPlaceLabel(const QString& text, const QPointF& pos,
                       MapCore::FeatureCategory category,
                       const TextStyle& style,
                       int priority,
                       QFontMetricsF& fm,
                       const QString& iconEmoji = "") {
        if (!viewportRect.contains(pos)) return false;

        qreal textWidth = fm.horizontalAdvance(text);
        qreal textHeight = fm.height();

        qreal pad = 4.0;
        qreal totalWidth = textWidth + (iconEmoji.isEmpty() ? 0 : 20.0) + pad * 2.0;
        qreal totalHeight = textHeight + pad * 2.0;

        // Position text centered or offset from point
        QRectF rect(pos.x() - totalWidth * 0.5, pos.y() + 6.0, totalWidth, totalHeight);

        // Check if out of bounds
        if (!viewportRect.contains(rect)) return false;

        // Collision check with already placed labels
        for (const auto& placed : placedLabels) {
            if (rect.intersects(placed.screenRect)) {
                return false;
            }
        }

        PlacedLabel lbl;
        lbl.text = text;
        lbl.iconEmoji = iconEmoji;
        lbl.screenPos = pos;
        lbl.screenRect = rect;
        lbl.category = category;
        lbl.style = style;
        lbl.priority = priority;

        placedLabels.push_back(lbl);
        return true;
    }

    void drawAll(QPainter& painter) const {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        // Pass 1: Draw halos / outlines
        for (const auto& lbl : placedLabels) {
            painter.setFont(lbl.style.font);

            // Draw text halo by drawing with offset or outline pen
            QPen haloPen(lbl.style.haloColor);
            haloPen.setWidthF(lbl.style.haloRadius * 2.0f);
            haloPen.setJoinStyle(Qt::RoundJoin);
            haloPen.setCapStyle(Qt::RoundCap);

            QPainterPath path;
            QPointF textPos = lbl.screenRect.topLeft() + QPointF(4.0, lbl.style.font.pointSizeF() + 2.0);
            path.addText(textPos, lbl.style.font, lbl.text);

            painter.strokePath(path, haloPen);
            painter.fillPath(path, QBrush(lbl.style.textColor));

            // Draw point marker / pin dot
            if (lbl.category == MapCore::FeatureCategory::PLACE_CITY) {
                painter.setBrush(QColor(234, 67, 53)); // Google Red
                painter.setPen(QPen(Qt::white, 2.0));
                painter.drawEllipse(lbl.screenPos, 4.5, 4.5);
            } else if (lbl.category == MapCore::FeatureCategory::PLACE_TOWN) {
                painter.setBrush(QColor(66, 133, 244)); // Google Blue
                painter.setPen(QPen(Qt::white, 1.5));
                painter.drawEllipse(lbl.screenPos, 3.5, 3.5);
            } else if (lbl.category == MapCore::FeatureCategory::POI_AIRPORT) {
                painter.setBrush(QColor(52, 168, 83)); // Green
                painter.setPen(QPen(Qt::white, 1.5));
                painter.drawEllipse(lbl.screenPos, 4.0, 4.0);
            } else if (lbl.category == MapCore::FeatureCategory::POI_HOSPITAL) {
                painter.setBrush(QColor(234, 67, 53)); // Red
                painter.setPen(QPen(Qt::white, 1.5));
                painter.drawEllipse(lbl.screenPos, 3.5, 3.5);
            }
        }

        painter.restore();
    }
};

} // namespace MapRenderer
