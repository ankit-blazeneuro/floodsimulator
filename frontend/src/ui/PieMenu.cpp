#include "PieMenu.h"
#include "IconHelper.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QKeyEvent>
#include <cmath>
#include <algorithm>

namespace MapUI {

PieMenu::PieMenu(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setMouseTracking(true);
    setFixedSize(270, 270);

    // 1. Move Tool (North / 90°) - Blender standard North slot
    items.push_back(PieMenuItem{
        MapTool::Move,
        "Move",
        "M",
        "map",
        QColor(138, 180, 248),
        90.0
    });

    // 2. Selector Tool (East / 0°) - Blender standard East slot
    items.push_back(PieMenuItem{
        MapTool::Select,
        "Select",
        "S",
        "search",
        QColor(253, 214, 99),
        0.0
    });

    // 3. Rotate Tool (South / 270°) - Blender standard South slot
    items.push_back(PieMenuItem{
        MapTool::Rotate,
        "Rotate",
        "T",
        "rewind-forward",
        QColor(84, 213, 154),
        270.0
    });

    // 4. Ruler Tool (West / 180°) - Blender standard West slot
    items.push_back(PieMenuItem{
        MapTool::Ruler,
        "Ruler",
        "R",
        "ruler",
        QColor(167, 139, 250),
        180.0
    });
}

void PieMenu::setActiveTool(MapTool tool) {
    currentActiveTool = tool;
    update();
}

void PieMenu::popup(const QPoint& globalCenterPos) {
    hoveredIndex = -1;
    move(globalCenterPos.x() - width() / 2, globalCenterPos.y() - height() / 2);
    show();
    setFocus();
    update();
}

static QRectF getEquidistantButtonRect(int index, double cx, double cy, double btnW, double btnH) {
    const double edgeGap = 38.0; // Exact equidistant radial gap from center to inner facing edge of every button

    if (index == 0) {
        // North (Move / 90°): Bottom edge faces center
        double bx = cx - btnW / 2.0;
        double by = cy - edgeGap - btnH;
        return QRectF(bx, by, btnW, btnH);
    } else if (index == 1) {
        // East (Select / 0°): Left edge faces center
        double bx = cx + edgeGap;
        double by = cy - btnH / 2.0;
        return QRectF(bx, by, btnW, btnH);
    } else if (index == 2) {
        // South (Rotate / 270°): Top edge faces center
        double bx = cx - btnW / 2.0;
        double by = cy + edgeGap;
        return QRectF(bx, by, btnW, btnH);
    } else {
        // West (Ruler / 180°): Right edge faces center
        double bx = cx - edgeGap - btnW;
        double by = cy - btnH / 2.0;
        return QRectF(bx, by, btnW, btnH);
    }
}

int PieMenu::getIndexUnderMouse(const QPoint& pos) const {
    double cx = width() / 2.0;
    double cy = height() / 2.0;
    double dx = pos.x() - cx;
    double dy = -(pos.y() - cy); // Invert so positive Y is upwards (North)

    const double btnW = 90.0;
    const double btnH = 28.0;

    // Check direct button bounding box intersection first
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (getEquidistantButtonRect(i, cx, cy, btnW, btnH).contains(pos)) {
            return i;
        }
    }

    double dist = std::sqrt(dx * dx + dy * dy);
    if (dist < 16.0) {
        return -1; // Center disc deadzone
    }

    double angle = std::atan2(dy, dx) * 180.0 / M_PI;
    if (angle < 0.0) angle += 360.0;

    // 4 sectors of 90° each:
    // Sector 0 (North / Move - 90°): [45°, 135°]
    // Sector 3 (West / Ruler - 180°): [135°, 225°]
    // Sector 2 (South / Rotate - 270°): [225°, 315°]
    // Sector 1 (East / Select - 0°): [315°, 360°] and [0°, 45°]
    if (angle >= 45.0 && angle < 135.0) {
        return 0; // Move
    } else if (angle >= 135.0 && angle < 225.0) {
        return 3; // Ruler
    } else if (angle >= 225.0 && angle < 315.0) {
        return 2; // Rotate
    } else {
        return 1; // Select
    }
}

void PieMenu::triggerSelection(int index) {
    if (index >= 0 && index < static_cast<int>(items.size())) {
        currentActiveTool = items[index].tool;
        emit toolSelected(currentActiveTool);
    }
    close();
}

void PieMenu::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    double cx = width() / 2.0;
    double cy = height() / 2.0;
    QPointF center(cx, cy);

    const double btnW = 90.0;
    const double btnH = 28.0;

    // 1. Blender-Style Directional Pointer Line to Hovered Button's facing edge
    if (hoveredIndex >= 0 && hoveredIndex < static_cast<int>(items.size())) {
        QRectF hRect = getEquidistantButtonRect(hoveredIndex, cx, cy, btnW, btnH);
        QPointF targetPt;
        if (hoveredIndex == 0) {
            targetPt = QPointF(cx, hRect.bottom());
        } else if (hoveredIndex == 1) {
            targetPt = QPointF(hRect.left(), cy);
        } else if (hoveredIndex == 2) {
            targetPt = QPointF(cx, hRect.top());
        } else {
            targetPt = QPointF(hRect.right(), cy);
        }

        // Direction line in Blender Cyan/Blue
        painter.setPen(QPen(QColor(71, 114, 179, 230), 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(center, targetPt);
    }

    // 2. Blender Center Disc Hub (Radius 15px)
    painter.setPen(QPen(QColor(70, 70, 70), 1.2));
    painter.setBrush(QColor(38, 38, 38, 245));
    painter.drawEllipse(center, 15.0, 15.0);

    // Inner Center Dot/Ring
    painter.setPen(QPen(QColor(140, 140, 140), 1.2));
    painter.setBrush(hoveredIndex >= 0 ? QColor(71, 114, 179) : QColor(65, 65, 65));
    painter.drawEllipse(center, 4.0, 4.0);

    // 3. Draw Radial Floating Tool Pill Buttons (Equidistant from inner edges)
    for (size_t i = 0; i < items.size(); ++i) {
        const auto& item = items[i];
        bool isHovered = (static_cast<int>(i) == hoveredIndex);
        bool isCurrentActive = (item.tool == currentActiveTool);

        QRectF btnRect = getEquidistantButtonRect(static_cast<int>(i), cx, cy, btnW, btnH);
        double bx = btnRect.x();
        double by = btnRect.y();

        // Blender Button Styling
        if (isHovered) {
            // Blender active blue
            painter.setBrush(QColor(71, 114, 179, 250));
            painter.setPen(QPen(QColor(122, 163, 230), 1.2));
        } else if (isCurrentActive) {
            // Active tool subtle highlight
            painter.setBrush(QColor(48, 48, 54, 240));
            painter.setPen(QPen(QColor(90, 115, 150), 1.0));
        } else {
            // Standard dark button
            painter.setBrush(QColor(38, 38, 42, 240));
            painter.setPen(QPen(QColor(24, 24, 26), 1.0));
        }

        painter.drawRoundedRect(btnRect, 4.0, 4.0);

        // Icon
        QPixmap iconPix = IconHelper::getPixmap(item.iconName,
            isHovered ? QColor(255, 255, 255) : item.iconColor, 15);
        if (!iconPix.isNull()) {
            painter.drawPixmap(static_cast<int>(bx + 7), static_cast<int>(by + (btnH - 15) / 2.0), iconPix);
        }

        // Title text
        painter.setPen(isHovered ? Qt::white : (isCurrentActive ? QColor(235, 235, 240) : QColor(190, 190, 195)));
        QFont btnFont("Segoe UI", 9, isHovered ? QFont::Bold : QFont::Medium);
        painter.setFont(btnFont);
        QRectF textRect(bx + 26, by, btnW - 30, btnH);
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, item.name);

        // Active indicator pip
        if (isCurrentActive && !isHovered) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(138, 180, 248));
            painter.drawEllipse(QPointF(bx + btnW - 8, by + btnH / 2.0), 2.5, 2.5);
        }
    }
}

void PieMenu::mouseMoveEvent(QMouseEvent* event) {
    int idx = getIndexUnderMouse(event->pos());
    if (idx != hoveredIndex) {
        hoveredIndex = idx;
        update();
    }
}

void PieMenu::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        int idx = getIndexUnderMouse(event->pos());
        if (idx >= 0) {
            triggerSelection(idx);
        } else {
            close();
        }
    }
}

void PieMenu::mouseReleaseEvent(QMouseEvent* event) {
    // Blender-style gesture release
    if (event->button() == Qt::RightButton) {
        int idx = getIndexUnderMouse(event->pos());
        if (idx >= 0) {
            triggerSelection(idx);
        }
    }
}

void PieMenu::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        close();
        return;
    }

    if (event->key() == Qt::Key_M || event->key() == Qt::Key_1) {
        triggerSelection(0); // Move
    } else if (event->key() == Qt::Key_S || event->key() == Qt::Key_2) {
        triggerSelection(1); // Select
    } else if (event->key() == Qt::Key_T || event->key() == Qt::Key_3 || event->key() == Qt::Key_O) {
        triggerSelection(2); // Rotate
    } else if (event->key() == Qt::Key_R || event->key() == Qt::Key_4) {
        triggerSelection(3); // Ruler
    } else {
        QWidget::keyPressEvent(event);
    }
}

void PieMenu::leaveEvent(QEvent* /*event*/) {
    hoveredIndex = -1;
    update();
}

} // namespace MapUI
