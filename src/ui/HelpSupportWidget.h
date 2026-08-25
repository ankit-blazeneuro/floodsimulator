#pragma once

#include <QWidget>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QComboBox>
#include <vector>
#include "../core/DamManager.h"
#include "OnlineTileWidget.h"
#include "NavigationControls.h"
#include "SearchBar.h"
#include "HelicopterDetailsPanel.h"
#include "../core/HelicopterTrackerManager.h"

namespace MapUI {

class HelpSupportWidget : public QWidget {
    Q_OBJECT

public:
    explicit HelpSupportWidget(QWidget* parent = nullptr);
    ~HelpSupportWidget() override = default;

    void setDamManager(const MapCore::DamManager* mgr);
    void setViewport(double lat, double lon, int zoom);
    double getCenterLat() const;
    double getCenterLon() const;
    int getZoom() const;

    void setTileProvider(MapCore::OnlineTileProvider provider);
    void setDarkMode(bool isDark);
    void setTool(MapTool tool);
    void setActive(bool active);

public slots:
    void zoomIn();
    void zoomOut();
    void resetToAssam();
    void resetToIndia();
    void toggleSidebar();
    void toggleHeliDetails();

signals:
    void syncRequested();
    void viewportChanged(double lat, double lon, int zoom);
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onHelpSearchChanged(const QString& query);

private:
    void setupUi();
    void buildHelpContent(QWidget* container);
    void applyDefaultSplitterSizes();
    void updateFloatingPositions();

    QSplitter* mainSplitter;
    QWidget* helpSidebar;
    OnlineTileWidget* referenceMap;
    HelicopterDetailsPanel* heliDetailsPanel;
    MapCore::HelicopterTrackerManager* heliManager;

    NavigationControls* navControls;
    SearchBar* searchBar;
    const MapCore::DamManager* currentDamManager = nullptr;

    QLineEdit* searchInput;
    std::vector<QWidget*> helpSectionCards;

    bool initialSplitterSizesSet = false;
    double sharedLat = 22.0;
    double sharedLon = 79.0;
    int sharedZoom = 5;
};

} // namespace MapUI
