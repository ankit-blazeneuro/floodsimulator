#pragma once

#include <QWidget>
#include <QScrollArea>
#include <QPushButton>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStackedWidget>
#include <QButtonGroup>

namespace MapUI {

class CollapsibleSection : public QWidget {
    Q_OBJECT

private:
    QPushButton* btnHeader;
    QWidget* contentWidget;
    QVBoxLayout* contentLayout;
    bool isCollapsed = false;

public:
    explicit CollapsibleSection(const QString& title, QWidget* parent = nullptr);

    QVBoxLayout* getContentLayout() { return contentLayout; }
    void addWidget(QWidget* w);

public slots:
    void toggleCollapse();
};

class PropertiesPanel : public QWidget {
    Q_OBJECT

private:
    QWidget* iconStrip;
    QButtonGroup* tabButtonGroup;
    QStackedWidget* pageStack;
    QLabel* lblPanelTitle;

    QPushButton* btnTabFluid;
    QPushButton* btnTabTerrain;
    QPushButton* btnTabTelemetry;
    QPushButton* btnTabDisplay;

    // Simulation parameter inputs
    QDoubleSpinBox* spinWaterRise;
    QDoubleSpinBox* spinRainfall;
    QDoubleSpinBox* spinBreachWidth;
    QDoubleSpinBox* spinVelocity;

    QPushButton* btnBakeSim;
    QPushButton* btnResetSim;

public:
    explicit PropertiesPanel(QWidget* parent = nullptr);

signals:
    void simulationBakeRequested(double waterRise, double rainfall, double breachWidth);
    void parameterChanged(const QString& name, double val);

private:
    void setupUi();
    QWidget* createFluidTab();
    QWidget* createTerrainTab();
    QWidget* createTelemetryTab();
    QWidget* createDisplayTab();
    void switchTab(int index, const QString& title);
};

} // namespace MapUI
