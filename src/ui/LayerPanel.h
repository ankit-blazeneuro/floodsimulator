#pragma once

#include <QWidget>
#include <QComboBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include "../renderer/MapStyle.h"
#include "../renderer/MapRenderer.h"

namespace MapUI {

class LayerPanel : public QWidget {
    Q_OBJECT

private:
    QComboBox* comboTheme;
    QCheckBox* checkBuildings;
    QCheckBox* checkRoads;
    QCheckBox* checkWater;
    QCheckBox* checkLanduse;
    QCheckBox* checkLabels;
    QCheckBox* checkPois;

    MapRenderer::RenderOptions currentOptions;

public:
    explicit LayerPanel(QWidget* parent = nullptr);

    void setOptions(const MapRenderer::RenderOptions& opt);
    const MapRenderer::RenderOptions& getOptions() const { return currentOptions; }

signals:
    void themeChanged(MapRenderer::ThemePreset preset);
    void optionsChanged(const MapRenderer::RenderOptions& options);

private:
    void setupUi();
};

} // namespace MapUI
