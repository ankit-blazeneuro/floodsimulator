#pragma once

#include <QWidget>
#include <QColor>
#include <vector>
#include "../core/DamManager.h"
#include "../core/DamFloodSimulation.h"

class QVBoxLayout;
class QLabel;

namespace MapUI {

// ─── Mini Hydrograph Widget (SVG-like custom paint) ──────────────────────────
class MiniHydrographWidget : public QWidget {
    Q_OBJECT
public:
    struct DataPoint {
        int minute;
        double discharge; // m³/s
    };

    explicit MiniHydrographWidget(const std::vector<DataPoint>& data, QColor lineColor, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<DataPoint> m_data;
    QColor m_lineColor;
};

// ─── Single Dam Simulation Card ──────────────────────────────────────────────
class DamSimCard : public QWidget {
    Q_OBJECT
public:
    explicit DamSimCard(const MapCore::DamPoint& dam, QWidget* parent = nullptr);

private:
    MapCore::DamPoint m_dam;
    void buildUi();
};

// ─── Main Hydro Flow Widget ──────────────────────────────────────────────────
class HydroFlowWidget : public QWidget {
    Q_OBJECT
public:
    explicit HydroFlowWidget(MapCore::DamManager* damManager = nullptr, QWidget* parent = nullptr);

    void setDamManager(MapCore::DamManager* mgr);
    void refresh();

private:
    MapCore::DamManager* m_damManager = nullptr;

    void rebuildUi();
    void buildUi();
};

} // namespace MapUI
