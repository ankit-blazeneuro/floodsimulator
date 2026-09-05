#pragma once

#include <QWidget>
#include <QPoint>
#include <QString>
#include <QIcon>
#include <vector>
#include "MapTool.h"

namespace MapUI {

struct PieMenuItem {
    MapTool tool;
    QString name;
    QString shortcutHint;
    QString iconName;
    QColor iconColor;
    double angleDeg; // 90 = North, 330 = South-East, 210 = South-West
};

class PieMenu : public QWidget {
    Q_OBJECT

private:
    std::vector<PieMenuItem> items;
    int hoveredIndex = -1;
    MapTool currentActiveTool = MapTool::Move;

    int outerRadius = 95;
    int innerRadius = 30;

public:
    explicit PieMenu(QWidget* parent = nullptr);

    void setActiveTool(MapTool tool);
    void popup(const QPoint& globalCenterPos);

signals:
    void toolSelected(MapTool tool);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int getIndexUnderMouse(const QPoint& pos) const;
    void triggerSelection(int index);
};

} // namespace MapUI
