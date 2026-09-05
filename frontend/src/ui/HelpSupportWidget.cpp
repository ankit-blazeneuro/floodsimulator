#include "HelpSupportWidget.h"
#include <QScrollBar>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QFrame>

namespace MapUI {

HelpSupportWidget::HelpSupportWidget(QWidget* parent)
    : QWidget(parent) {
    setObjectName("helpSupportRoot");
    setStyleSheet("QWidget#helpSupportRoot { background-color: #121215; }");
    setupUi();
}

void HelpSupportWidget::setupUi() {
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    mainSplitter = new QSplitter(Qt::Horizontal, this);
    mainSplitter->setHandleWidth(2);
    mainSplitter->setStyleSheet(R"(
        QSplitter::handle:horizontal {
            background-color: #27272A;
            width: 2px;
        }
        QSplitter::handle:horizontal:hover {
            background-color: #F59E0B;
        }
    )");

    // 1. Left Support Portal Sidebar (Scrollable)
    helpSidebar = new QWidget(mainSplitter);
    helpSidebar->setObjectName("helpSidebar");
    helpSidebar->setStyleSheet(R"(
        QWidget#helpSidebar {
            background-color: #18181B;
            border-right: 1px solid #27272A;
        }
        QScrollArea {
            background-color: transparent;
            border: none;
        }
        QScrollBar:vertical {
            background: #18181B;
            width: 6px;
            margin: 0px;
        }
        QScrollBar::handle:vertical {
            background: #3F3F46;
            min-height: 20px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical:hover {
            background: #71717A;
        }
        QLabel {
            font-family: 'Segoe UI', Inter, -apple-system, sans-serif;
            color: #F4F4F5;
            background: transparent;
            border: none;
        }
        QLineEdit {
            background-color: #27272A;
            color: #F4F4F5;
            border: 1px solid #3F3F46;
            border-radius: 6px;
            padding: 6px 10px;
            font-family: 'Segoe UI', Inter, sans-serif;
            font-size: 11px;
        }
        QLineEdit:focus {
            border: 1px solid #F59E0B;
            background-color: #18181B;
        }
    )");

    auto* sidebarLayout = new QVBoxLayout(helpSidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    auto* scrollArea = new QScrollArea(helpSidebar);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* contentWidget = new QWidget(scrollArea);
    buildHelpContent(contentWidget);
    scrollArea->setWidget(contentWidget);

    sidebarLayout->addWidget(scrollArea);
    mainSplitter->addWidget(helpSidebar);
    helpSidebar->hide(); // Hidden by default as requested

    // 2. Right Reference Map Container
    auto* mapContainer = new QWidget(mainSplitter);
    auto* mapLayout = new QVBoxLayout(mapContainer);
    mapLayout->setContentsMargins(0, 0, 0, 0);
    mapLayout->setSpacing(0);

    referenceMap = new OnlineTileWidget(mapContainer);
    referenceMap->setTileProvider(MapCore::OnlineTileProvider::OpenStreetMap_Dark);
    referenceMap->setCenter(sharedLat, sharedLon);
    referenceMap->setZoom(sharedZoom);
    referenceMap->setShowDams(false); // Do not map dams on help screen
    connect(referenceMap, &OnlineTileWidget::viewportChanged, this, [this](double lat, double lon, int zoom) {
        sharedLat = lat;
        sharedLon = lon;
        sharedZoom = zoom;
        emit viewportChanged(lat, lon, zoom);
    });
    connect(referenceMap, &OnlineTileWidget::contextMenuRequested, this, &HelpSupportWidget::contextMenuRequested);
    mapLayout->addWidget(referenceMap, 1);

    // Floating Navigation Controls (Same as simulation screen - bottom right)
    navControls = new NavigationControls(referenceMap);
    connect(navControls, &NavigationControls::zoomInRequested, this, &HelpSupportWidget::zoomIn);
    connect(navControls, &NavigationControls::zoomOutRequested, this, &HelpSupportWidget::zoomOut);
    connect(navControls, &NavigationControls::fitExtentRequested, this, &HelpSupportWidget::resetToAssam);
    connect(navControls, &NavigationControls::resetNorthRequested, this, &HelpSupportWidget::resetToIndia);

    // Floating Search Bar (Same as simulation screen - top left)
    searchBar = new SearchBar(referenceMap);
    if (currentDamManager) {
        searchBar->setDamManager(currentDamManager);
    }
    connect(searchBar, &SearchBar::searchResultSelected, this, [this](MapCore::Point2D pos, float targetZoom, QString name, QString detail, MapCore::FeatureCategory category) {
        Q_UNUSED(name);
        Q_UNUSED(detail);
        Q_UNUSED(category);
        setViewport(pos.y, pos.x, static_cast<int>(targetZoom));
        emit viewportChanged(pos.y, pos.x, static_cast<int>(targetZoom));
    });
    connect(searchBar, &SearchBar::damResultSelected, this, [this](const MapCore::DamPoint& dam) {
        setViewport(dam.lat, dam.lon, 11);
        emit viewportChanged(dam.lat, dam.lon, 11);
    });

    mainSplitter->addWidget(mapContainer);

    // 3. Right Helicopter SAR Details Panel (Live Telemetry & Fleet Stream)
    heliDetailsPanel = new HelicopterDetailsPanel(mainSplitter);
    mainSplitter->addWidget(heliDetailsPanel);
    heliDetailsPanel->show();

    referenceMap->setShowHelicopters(true);

    // Live ADS-B Real-Time Helicopter Tracking Manager (3-second delay max)
    heliManager = new MapCore::HelicopterTrackerManager(this);
    connect(heliManager, &MapCore::HelicopterTrackerManager::helicoptersUpdated, this, [this](const std::vector<MapCore::HelicopterTrack>& list) {
        if (referenceMap) {
            referenceMap->setHelicopters(list);
        }
        if (heliDetailsPanel) {
            heliDetailsPanel->updateHelicopterList(list);
        }
    });

    connect(heliManager, &MapCore::HelicopterTrackerManager::trackingStatusChanged, this, [this](bool, int, const QString& status) {
        if (heliDetailsPanel) {
            heliDetailsPanel->setLiveStatus(status);
        }
    });

    connect(referenceMap, &OnlineTileWidget::helicopterClicked, this, [this](const MapCore::HelicopterTrack& heli) {
        if (heliDetailsPanel) {
            heliDetailsPanel->show();
            heliDetailsPanel->setHelicopter(heli);
        }
        if (referenceMap) {
            referenceMap->setSelectedHelicopterHex(heli.hex);
        }
    });

    connect(heliDetailsPanel, &HelicopterDetailsPanel::helicopterSelectedFromList, this, [this](const QString& hex) {
        if (referenceMap) {
            referenceMap->setSelectedHelicopterHex(hex);
        }
        if (heliManager) {
            const auto* h = heliManager->getHelicopter(hex);
            if (h && referenceMap) {
                referenceMap->setCenter(h->lat, h->lon);
                heliDetailsPanel->setHelicopter(*h);
            }
        }
    });

    connect(heliDetailsPanel, &HelicopterDetailsPanel::centerMapRequested, this, [this](double lat, double lon) {
        setViewport(lat, lon, 11);
        emit viewportChanged(lat, lon, 11);
    });

    connect(heliDetailsPanel, &HelicopterDetailsPanel::closeRequested, this, [this]() {
        if (heliDetailsPanel) {
            heliDetailsPanel->hide();
            applyDefaultSplitterSizes();
        }
    });

    // Start 3-second live ADS-B flight telemetry stream
    heliManager->setTrackingCenter(sharedLat, sharedLon, 250.0);
    heliManager->startTracking(3000);

    rootLayout->addWidget(mainSplitter);
}

void HelpSupportWidget::buildHelpContent(QWidget* container) {
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(14, 14, 14, 20);
    layout->setSpacing(12);

    // ==========================================
    // 1. Header Row
    // ==========================================
    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(6);
    auto* lblIcon = new QLabel("🎧", container);
    lblIcon->setStyleSheet("font-size: 18px;");
    headerRow->addWidget(lblIcon);

    auto* lblTitle = new QLabel("Help & Support Portal", container);
    lblTitle->setStyleSheet("font-size: 14px; font-weight: bold; color: #FBBF24;");
    headerRow->addWidget(lblTitle, 1);
    layout->addLayout(headerRow);

    // Subtitle
    auto* lblSub = new QLabel("Documentation, GIS guides, shortcuts, and emergency disaster contacts.", container);
    lblSub->setWordWrap(true);
    lblSub->setStyleSheet("font-size: 11px; color: #A1A1AA; line-height: 1.4;");
    layout->addWidget(lblSub);

    // ==========================================
    // 2. Search Box
    // ==========================================
    searchInput = new QLineEdit(container);
    searchInput->setPlaceholderText("Search help topics, tools, shortcuts...");
    searchInput->setClearButtonEnabled(true);
    connect(searchInput, &QLineEdit::textChanged, this, &HelpSupportWidget::onHelpSearchChanged);
    layout->addWidget(searchInput);

    auto createSectionCard = [this, container, layout](const QString& title, const QString& contentText, const QString& actionText = "", double flyLat = 0, double flyLon = 0, int flyZoom = 8) -> QWidget* {
        auto* card = new QWidget(container);
        card->setStyleSheet("QWidget { background-color: #1E1E24; border: 1px solid #2E2E38; border-radius: 8px; }");
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        cardLayout->setSpacing(6);

        auto* lblHead = new QLabel(title, card);
        lblHead->setStyleSheet("font-size: 12px; font-weight: 700; color: #FBBF24;");
        cardLayout->addWidget(lblHead);

        auto* lblBody = new QLabel(contentText, card);
        lblBody->setWordWrap(true);
        lblBody->setStyleSheet("font-size: 11px; color: #D4D4D8; line-height: 1.4;");
        cardLayout->addWidget(lblBody);

        if (!actionText.isEmpty()) {
            auto* btnFly = new QPushButton(actionText, card);
            btnFly->setStyleSheet("QPushButton { background-color: rgba(245, 158, 11, 0.15); color: #FBBF24; border: 1px solid rgba(245, 158, 11, 0.40); border-radius: 4px; font-size: 10px; font-weight: 600; padding: 4px 8px; } QPushButton:hover { background-color: #F59E0B; color: #09090B; }");
            connect(btnFly, &QPushButton::clicked, this, [this, flyLat, flyLon, flyZoom]() {
                if (referenceMap) {
                    referenceMap->setCenter(flyLat, flyLon);
                    referenceMap->setZoom(flyZoom);
                }
            });
            cardLayout->addWidget(btnFly, 0, Qt::AlignLeft);
        }

        layout->addWidget(card);
        helpSectionCards.push_back(card);
        return card;
    };

    // ==========================================
    // 3. Guide Sections
    // ==========================================

    // Section 1: Dam Simulation
    createSectionCard(
        "🌊 Hydrodynamic Dam Flood Simulation",
        "• Click any dam point on the map to trigger a 60-minute hydrodynamic breach model.\n"
        "• Adjust discharge rate (0 to 12,000 m³/s) and dam breach width in real-time.\n"
        "• Use the bottom timeline to scrub minute-by-minute flood inundation wavefronts.\n"
        "• Downstream hazard buffers: Red (<15m arrival), Orange (<30m), Yellow (<60m).",
        "📍 Fly to Karbi Langpi Dam (Assam)",
        25.9866, 92.5188, 11
    );

    // Section 2: Weather Forecast
    createSectionCard(
        "🌦️ Weather Intelligence & Radar",
        "• View real-time Open-Meteo Doppler radar, thermal heatmaps, and wind velocity.\n"
        "• Scrub through the 24-hour horizon or inspect the 7-day extended outlook.\n"
        "• Severe rainfall alerts automatically trigger when precipitation exceeds 5 mm/h.\n"
        "• One-click quick region presets for Guwahati, Tezpur, Dibrugarh, Delhi, and Mumbai.",
        "📍 Fly to Subansiri Lower Dam",
        27.5540, 94.2580, 10
    );

    // Section 3: GIS Tools
    createSectionCard(
        "📐 GIS Measurement & Layer Tools",
        "• Ruler Tool: Click points on the map to measure geodesical distance via Haversine formula.\n"
        "• Select Tool: Drag a box on the canvas to multi-select and query dams.\n"
        "• Rotate Tool: Click and drag around the center to rotate the map 360°.\n"
        "• Move Tool: Left-drag to pan the map, middle-drag for universal pan.",
        "📍 Fly to Guwahati Urban Basin",
        26.1445, 91.7362, 10
    );

    // Section 4: Keyboard Shortcuts
    createSectionCard(
        "⌨️ Essential Keyboard Shortcuts",
        "• Ctrl + + / Ctrl + = : Zoom In on Map\n"
        "• Ctrl + - / Ctrl + _ : Zoom Out on Map\n"
        "• Spacebar : Play / Pause Simulation & Weather Timeline\n"
        "• Ctrl + K : Quick focus search bar\n"
        "• Esc : Cancel measurement ruler or deselect active dam\n"
        "• Double Click : Center and zoom directly into target location",
        ""
    );

    // Section 5: System Diagnostics
    createSectionCard(
        "🛠️ System Diagnostics & Engine Status",
        "• OSM & CartoDB Tile Cache: Active (3,000 tile LRU capacity)\n"
        "• Open-Meteo API Engine: Online (Automatic timezone resolution)\n"
        "• Spatial Index: R-Tree Indexed (10 Major Assam / NE Hydro Dams)\n"
        "• Graphics Renderer: 60 FPS Hardware-Accelerated QPainter Canvas",
        ""
    );

    // Section 6: Emergency Dispatch
    createSectionCard(
        "📞 Emergency Dispatch & Contacts",
        "• Assam State Disaster Management (ASDMA): 1070 / 1079\n"
        "• National Disaster Response Force (NDRF): 011-24363260\n"
        "• Central Water Commission (CWC Flood Control): 011-26105593\n"
        "• Emergency Dispatch Email: emergency.dispatch@redr.gov.in",
        ""
    );

    layout->addStretch(1);
}

void HelpSupportWidget::onHelpSearchChanged(const QString& query) {
    QString q = query.trimmed().toLower();
    for (auto* card : helpSectionCards) {
        if (!card) continue;
        if (q.isEmpty()) {
            card->show();
            continue;
        }

        bool match = false;
        const auto labels = card->findChildren<QLabel*>();
        for (auto* lbl : labels) {
            if (lbl && lbl->text().toLower().contains(q)) {
                match = true;
                break;
            }
        }
        card->setVisible(match);
    }
}

void HelpSupportWidget::setDamManager(const MapCore::DamManager* mgr) {
    currentDamManager = mgr;
    if (referenceMap) {
        referenceMap->setShowDams(false);
    }
    if (searchBar) {
        searchBar->setDamManager(mgr);
    }
}

void HelpSupportWidget::setViewport(double lat, double lon, int zoom) {
    sharedLat = std::clamp(lat, -85.0511, 85.0511);
    sharedLon = std::clamp(lon, -180.0, 180.0);
    sharedZoom = std::clamp(zoom, 2, 19);
    if (referenceMap) {
        referenceMap->blockSignals(true);
        referenceMap->setCenter(sharedLat, sharedLon);
        referenceMap->setZoom(sharedZoom);
        referenceMap->blockSignals(false);
    }
    if (heliManager) {
        heliManager->setTrackingCenter(sharedLat, sharedLon, 250.0);
    }
}

double HelpSupportWidget::getCenterLat() const {
    return referenceMap ? referenceMap->getCenterLat() : sharedLat;
}

double HelpSupportWidget::getCenterLon() const {
    return referenceMap ? referenceMap->getCenterLon() : sharedLon;
}

int HelpSupportWidget::getZoom() const {
    return referenceMap ? referenceMap->getZoom() : sharedZoom;
}

void HelpSupportWidget::toggleSidebar() {
    if (!helpSidebar) return;
    bool show = !helpSidebar->isVisible();
    helpSidebar->setVisible(show);
    applyDefaultSplitterSizes();
}

void HelpSupportWidget::toggleHeliDetails() {
    if (!heliDetailsPanel) return;
    bool show = !heliDetailsPanel->isVisible();
    heliDetailsPanel->setVisible(show);
    applyDefaultSplitterSizes();
}

void HelpSupportWidget::setTileProvider(MapCore::OnlineTileProvider provider) {
    if (referenceMap) {
        referenceMap->setTileProvider(provider);
    }
}

void HelpSupportWidget::setDarkMode(bool isDark) {
    if (referenceMap) {
        referenceMap->setDarkMode(isDark);
    }
}

void HelpSupportWidget::setTool(MapTool tool) {
    if (referenceMap) {
        referenceMap->setTool(tool);
    }
}

void HelpSupportWidget::setActive(bool active) {
    if (referenceMap) {
        referenceMap->setRenderingActive(active);
    }
    if (heliManager) {
        if (active) {
            heliManager->setTrackingCenter(sharedLat, sharedLon, 250.0);
            heliManager->startTracking(3000);
        } else {
            heliManager->stopTracking();
        }
    }
}

void HelpSupportWidget::zoomIn() {
    if (referenceMap) {
        referenceMap->zoomIn();
        sharedZoom = referenceMap->getZoom();
        emit viewportChanged(sharedLat, sharedLon, sharedZoom);
    }
}

void HelpSupportWidget::zoomOut() {
    if (referenceMap) {
        referenceMap->zoomOut();
        sharedZoom = referenceMap->getZoom();
        emit viewportChanged(sharedLat, sharedLon, sharedZoom);
    }
}

void HelpSupportWidget::resetToAssam() {
    if (referenceMap) {
        referenceMap->fitAssam();
        sharedLat = referenceMap->getCenterLat();
        sharedLon = referenceMap->getCenterLon();
        sharedZoom = referenceMap->getZoom();
        emit viewportChanged(sharedLat, sharedLon, sharedZoom);
    }
}

void HelpSupportWidget::resetToIndia() {
    if (referenceMap) {
        referenceMap->fitIndia();
        sharedLat = referenceMap->getCenterLat();
        sharedLon = referenceMap->getCenterLon();
        sharedZoom = referenceMap->getZoom();
        emit viewportChanged(sharedLat, sharedLon, sharedZoom);
    }
}

void HelpSupportWidget::updateFloatingPositions() {
    if (navControls && referenceMap) {
        int w = referenceMap->width();
        int h = referenceMap->height();
        navControls->move(w - navControls->width() - 20, h - navControls->height() - 20);
    }
    if (searchBar && referenceMap) {
        searchBar->move(20, 20);
    }
}

void HelpSupportWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!initialSplitterSizesSet && width() > 100) {
        initialSplitterSizesSet = true;
        applyDefaultSplitterSizes();
    }
    updateFloatingPositions();
}

void HelpSupportWidget::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!initialSplitterSizesSet && width() > 100) {
        initialSplitterSizesSet = true;
        applyDefaultSplitterSizes();
    }
    updateFloatingPositions();
}

void HelpSupportWidget::applyDefaultSplitterSizes() {
    int totalW = width();
    if (totalW > 100) {
        int leftW = (helpSidebar && helpSidebar->isVisible()) ? std::max(280, totalW / 4) : 0;
        int rightW = (heliDetailsPanel && heliDetailsPanel->isVisible()) ? 340 : 0;
        int mapW = std::max(300, totalW - leftW - rightW);
        mainSplitter->setSizes({ leftW, mapW, rightW });
    }
}

void HelpSupportWidget::keyPressEvent(QKeyEvent* event) {
    if (event->modifiers() & Qt::ControlModifier) {
        int key = event->key();
        if (key == Qt::Key_Plus || key == Qt::Key_Equal || event->text() == "+") {
            zoomIn();
            event->accept();
            return;
        } else if (key == Qt::Key_Minus || key == Qt::Key_Underscore || event->text() == "-") {
            zoomOut();
            event->accept();
            return;
        }
    }
    QWidget::keyPressEvent(event);
}

} // namespace MapUI
