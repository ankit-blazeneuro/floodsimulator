#include "MapWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <cmath>

namespace MapUI {

MapWidget::MapWidget(QWidget* parent) : QWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);

    // Initial center at Guwahati / Assam (lat 26.2, lon 92.5)
    center = MapCore::Projection::geoToMercator(MapCore::GeoCoord(26.2, 92.5));
    zoomLevel = 8.5f;
    targetZoom = 8.5f;

    animTimer = new QTimer(this);
    animTimer->setInterval(16); // ~60 FPS
    connect(animTimer, &QTimer::timeout, this, &MapWidget::onAnimationTick);
    animTimer->start();

    lastFpsTime = QDateTime::currentMSecsSinceEpoch();
}

void MapWidget::setSpatialIndex(const MapCore::SpatialIndex* index) {
    spatialIndex = index;
    if (spatialIndex && spatialIndex->extent.isValid()) {
        fitAssam();
    }
    update();
}

void MapWidget::setCenter(const MapCore::Point2D& c) {
    center = c;
    updateViewportNotification();
    update();
}

void MapWidget::setZoom(float z) {
    zoomLevel = std::clamp(z, 4.0f, 19.0f);
    targetZoom = zoomLevel;
    updateViewportNotification();
    update();
}

void MapWidget::zoomIn() {
    targetZoom = std::clamp(targetZoom + 1.0f, 4.0f, 19.0f);
}

void MapWidget::zoomOut() {
    targetZoom = std::clamp(targetZoom - 1.0f, 4.0f, 19.0f);
}

void MapWidget::fitExtent(const MapCore::BoundingBox& box) {
    if (!box.isValid()) return;

    center = box.center();

    double scaleX = (width() > 0 ? (width() * 0.85) / box.width() : 256.0);
    double scaleY = (height() > 0 ? (height() * 0.85) / box.height() : 256.0);
    double scale = std::min(scaleX, scaleY);

    float calculatedZoom = static_cast<float>(std::log2(scale / 256.0));
    setZoom(calculatedZoom);
}

void MapWidget::fitAssam() {
    if (spatialIndex && spatialIndex->extent.isValid()) {
        fitExtent(spatialIndex->extent);
    } else {
        setCenter(MapCore::Projection::geoToMercator(MapCore::GeoCoord(26.2, 92.5)));
        setZoom(8.2f);
    }
}

void MapWidget::flyTo(const MapCore::Point2D& targetCenter, float targetZoomLevel, qint64 durationMs) {
    isAnimatingFlyTo = true;
    flyStartCenter = center;
    flyEndCenter = targetCenter;
    flyStartZoom = zoomLevel;
    flyEndZoom = std::clamp(targetZoomLevel, 4.0f, 19.0f);
    targetZoom = flyEndZoom;
    flyDurationMs = durationMs;
    flyStartTime = QDateTime::currentMSecsSinceEpoch();
}

void MapWidget::setSelectedPosition(const MapCore::Point2D& pos) {
    selectedPos = pos;
    update();
}

void MapWidget::clearSelection() {
    selectedPos = MapCore::Point2D(-1, -1);
    update();
}

void MapWidget::setMeasureMode(bool active) {
    measureMode = active;
    if (!active) {
        clearMeasure();
    }
    setCursor(active ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

void MapWidget::clearMeasure() {
    measurePoints.clear();
    update();
}

float MapWidget::easeInOutCubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

void MapWidget::onAnimationTick() {
    bool needsRepaint = false;

    // 1. Smooth Camera FlyTo animation
    if (isAnimatingFlyTo) {
        qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - flyStartTime;
        float progress = std::clamp(static_cast<float>(elapsed) / flyDurationMs, 0.0f, 1.0f);
        float ease = easeInOutCubic(progress);

        center.x = flyStartCenter.x + (flyEndCenter.x - flyStartCenter.x) * ease;
        center.y = flyStartCenter.y + (flyEndCenter.y - flyStartCenter.y) * ease;
        zoomLevel = flyStartZoom + (flyEndZoom - flyStartZoom) * ease;

        if (progress >= 1.0f) {
            isAnimatingFlyTo = false;
            center = flyEndCenter;
            zoomLevel = flyEndZoom;
        }
        needsRepaint = true;
        updateViewportNotification();
    }
    // 2. Smooth Zoom interpolation
    else if (std::abs(zoomLevel - targetZoom) > 0.005f) {
        zoomLevel += (targetZoom - zoomLevel) * 0.25f;
        needsRepaint = true;
        updateViewportNotification();
    }

    // 3. Inertial Pan Momentum
    if (!isDragging && !isAnimatingFlyTo && (std::abs(panVelocity.x()) > 0.1 || std::abs(panVelocity.y()) > 0.1)) {
        double scale = MapRenderer::MapRenderer::calculateScale(zoomLevel);
        center.x -= static_cast<float>(panVelocity.x() / scale);
        center.y -= static_cast<float>(panVelocity.y() / scale);

        panVelocity *= 0.88; // Friction deceleration
        if (std::abs(panVelocity.x()) < 0.1 && std::abs(panVelocity.y()) < 0.1) {
            panVelocity = QPointF(0, 0);
        }
        needsRepaint = true;
        updateViewportNotification();
    }

    // Calculate FPS
    frameCount++;
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastFpsTime >= 1000) {
        currentFps = frameCount * 1000.0f / (now - lastFpsTime);
        frameCount = 0;
        lastFpsTime = now;
        emit fpsChanged(currentFps);
    }

    if (needsRepaint) {
        update();
    }
}

void MapWidget::updateViewportNotification() {
    MapCore::BoundingBox viewBbox = MapRenderer::MapRenderer::getViewportMercatorBbox(center, zoomLevel, width(), height());
    MapCore::GeoCoord geo = MapCore::Projection::mercatorToGeo(center);
    emit viewportChanged(viewBbox, zoomLevel, geo);
}

void MapWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    if (spatialIndex) {
        renderer.render(painter, *spatialIndex, center, zoomLevel, width(), height(), selectedPos, measurePoints);
    } else {
        painter.fillRect(rect(), renderer.getStyle().backgroundColor);
    }
}

void MapWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        lastMousePos = event->pos();
        panVelocity = QPointF(0, 0);
    } else if (event->button() == Qt::RightButton) {
        if (measureMode) {
            if (!measurePoints.empty()) {
                measurePoints.pop_back();
                update();
            }
        } else {
            clearSelection();
        }
    }
}

void MapWidget::mouseMoveEvent(QMouseEvent* event) {
    if (isDragging) {
        QPoint delta = event->pos() - lastMousePos;
        lastMousePos = event->pos();

        panVelocity = QPointF(delta.x(), delta.y());

        double scale = MapRenderer::MapRenderer::calculateScale(zoomLevel);
        center.x -= static_cast<float>(delta.x() / scale);
        center.y -= static_cast<float>(delta.y() / scale);

        updateViewportNotification();
        update();
    }

    // Hover queries
    MapCore::Point2D curMerc = MapRenderer::MapRenderer::screenToMercator(event->pos(), center, zoomLevel, width(), height());
    MapCore::GeoCoord curGeo = MapCore::Projection::mercatorToGeo(curMerc);

    QString hoverText;
    if (spatialIndex) {
        float maxSearchMerc = static_cast<float>(15.0 / MapRenderer::MapRenderer::calculateScale(zoomLevel));
        MapCore::FeatureInfo nearest = spatialIndex->findNearest(curMerc, maxSearchMerc, zoomLevel);
        if (nearest.found && !nearest.name.empty()) {
            hoverText = QString("%1: %2").arg(QString::fromUtf8(MapCore::getCategoryDisplayName(nearest.category)))
                                         .arg(QString::fromStdString(nearest.name));
        }
    }

    emit cursorGeoMoved(curGeo, hoverText);
}

void MapWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        isDragging = false;

        // If it was a click without dragging
        if (panVelocity.manhattanLength() < 5) {
            MapCore::Point2D clickMerc = MapRenderer::MapRenderer::screenToMercator(event->pos(), center, zoomLevel, width(), height());

            if (measureMode) {
                measurePoints.push_back(clickMerc);
                update();
            } else if (spatialIndex) {
                float maxSearchMerc = static_cast<float>(20.0 / MapRenderer::MapRenderer::calculateScale(zoomLevel));
                MapCore::FeatureInfo nearest = spatialIndex->findNearest(clickMerc, maxSearchMerc, zoomLevel);

                if (nearest.found) {
                    selectedPos = nearest.mercatorPos;
                    emit featureSelected(nearest);
                } else {
                    selectedPos = clickMerc;
                    MapCore::FeatureInfo rawLoc;
                    rawLoc.found = true;
                    rawLoc.category = MapCore::FeatureCategory::UNKNOWN;
                    rawLoc.name = "Dropped Pin";
                    rawLoc.detail = "Geographic Point";
                    rawLoc.mercatorPos = clickMerc;
                    rawLoc.geoCoord = MapCore::Projection::mercatorToGeo(clickMerc);
                    emit featureSelected(rawLoc);
                }
                update();
            }
        }
    }
}

void MapWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && !measureMode) {
        // Zoom in centered at double-click point
        MapCore::Point2D clickMerc = MapRenderer::MapRenderer::screenToMercator(event->pos(), center, zoomLevel, width(), height());
        flyTo(clickMerc, zoomLevel + 1.2f, 400);
    }
}

void MapWidget::wheelEvent(QWheelEvent* event) {
    float rawDelta = static_cast<float>(event->angleDelta().y());
    if (invertScroll) rawDelta = -rawDelta;

    float zoomDelta = (rawDelta / 320.0f) * static_cast<float>(zoomSensitivity);
    if (std::abs(zoomDelta) < 0.001f) return;

    if (anchorZoomToCursor) {
        // Anchor zoom to cursor position
        QPointF mousePos = event->position();
        MapCore::Point2D mouseMercBefore = MapRenderer::MapRenderer::screenToMercator(mousePos, center, zoomLevel, width(), height());

        targetZoom = std::clamp(targetZoom + zoomDelta, 4.0f, 19.0f);
        zoomLevel = std::clamp(zoomLevel + zoomDelta * 0.5f, 4.0f, 19.0f);

        // Adjust center so mouseMercBefore stays under mousePos
        double scaleNew = MapRenderer::MapRenderer::calculateScale(zoomLevel);
        center.x = static_cast<float>(mouseMercBefore.x - (mousePos.x() - width() * 0.5) / scaleNew);
        center.y = static_cast<float>(mouseMercBefore.y - (mousePos.y() - height() * 0.5) / scaleNew);
    } else {
        targetZoom = std::clamp(targetZoom + zoomDelta, 4.0f, 19.0f);
        zoomLevel = std::clamp(zoomLevel + zoomDelta * 0.5f, 4.0f, 19.0f);
    }

    updateViewportNotification();
    update();
}

void MapWidget::resizeEvent(QResizeEvent* /*event*/) {
    updateViewportNotification();
}

void MapWidget::keyPressEvent(QKeyEvent* event) {
    double scale = MapRenderer::MapRenderer::calculateScale(zoomLevel);
    float panStep = static_cast<float>(60.0 / scale);

    switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_A:
            center.x -= panStep;
            updateViewportNotification();
            update();
            break;
        case Qt::Key_Right:
        case Qt::Key_D:
            center.x += panStep;
            updateViewportNotification();
            update();
            break;
        case Qt::Key_Up:
        case Qt::Key_W:
            center.y -= panStep;
            updateViewportNotification();
            update();
            break;
        case Qt::Key_Down:
        case Qt::Key_S:
            center.y += panStep;
            updateViewportNotification();
            update();
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            zoomIn();
            break;
        case Qt::Key_Minus:
        case Qt::Key_Underscore:
            zoomOut();
            break;
        case Qt::Key_Home:
        case Qt::Key_0:
            fitAssam();
            break;
        case Qt::Key_Escape:
            clearSelection();
            if (measureMode) setMeasureMode(false);
            break;
        default:
            QWidget::keyPressEvent(event);
            break;
    }
}

} // namespace MapUI
