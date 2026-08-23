#include "SearchBar.h"
#include "IconHelper.h"
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QKeyEvent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>
#include <algorithm>

namespace MapUI {

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    networkManager = new QNetworkAccessManager(this);
    debounceTimer = new QTimer(this);
    debounceTimer->setSingleShot(true);
    debounceTimer->setInterval(280); // 280 ms debounce for smooth typing

    connect(debounceTimer, &QTimer::timeout, this, &SearchBar::onDebounceTimeout);
    connect(networkManager, &QNetworkAccessManager::finished, this, &SearchBar::onGeocodeReply);

    initStaticLocations();
    setupUi();
}

void SearchBar::setSearchIndex(const std::vector<MapCore::SearchItem>& index) {
    searchIndex = &index;
}

void SearchBar::setDamManager(const MapCore::DamManager* mgr) {
    damManager = mgr;
}

void SearchBar::initStaticLocations() {
    staticIndiaLocations.clear();

    auto addCity = [this](const QString& name, const QString& state, double lat, double lon, float zoom = 12.0f) {
        SearchResultItem item;
        item.title = name;
        item.subtitle = QString("City · %1, India").arg(state);
        item.iconEmoji = "🏙️";
        item.lat = lat;
        item.lon = lon;
        item.zoomTarget = zoom;
        item.category = MapCore::FeatureCategory::PLACE_CITY;
        item.isDam = false;
        staticIndiaLocations.push_back(item);
    };

    auto addRiver = [this](const QString& name, const QString& region, double lat, double lon, float zoom = 10.0f) {
        SearchResultItem item;
        item.title = name;
        item.subtitle = QString("River · %1").arg(region);
        item.iconEmoji = "🌊";
        item.lat = lat;
        item.lon = lon;
        item.zoomTarget = zoom;
        item.category = MapCore::FeatureCategory::WATER_RIVER;
        item.isDam = false;
        staticIndiaLocations.push_back(item);
    };

    // Assam & Northeast Cities & Towns
    addCity("Guwahati", "Assam", 26.1445, 91.7362, 12.5f);
    addCity("Dispur", "Assam (Capital)", 26.1433, 91.7898, 13.0f);
    addCity("Dibrugarh", "Assam", 27.4728, 94.9120, 12.5f);
    addCity("Silchar", "Assam", 24.8333, 92.7789, 12.5f);
    addCity("Jorhat", "Assam", 26.7509, 94.2037, 12.5f);
    addCity("Tezpur", "Assam", 26.6338, 92.7926, 12.5f);
    addCity("Nagaon", "Assam", 26.3452, 92.6840, 12.5f);
    addCity("Tinsukia", "Assam", 27.5000, 95.3667, 12.5f);
    addCity("Bongaigaon", "Assam", 26.5000, 90.5500, 12.5f);
    addCity("Kaziranga National Park", "Assam", 26.5775, 93.1711, 11.5f);
    addCity("Majuli Island", "Assam", 26.9500, 94.1667, 11.0f);
    addCity("Shillong", "Meghalaya", 25.5788, 91.8933, 12.5f);
    addCity("Itanagar", "Arunachal Pradesh", 27.0844, 93.6053, 12.5f);
    addCity("Imphal", "Manipur", 24.8170, 93.9368, 12.5f);
    addCity("Aizawl", "Mizoram", 23.7271, 92.7176, 12.5f);
    addCity("Kohima", "Nagaland", 25.6751, 94.1086, 12.5f);
    addCity("Agartala", "Tripura", 23.8315, 91.2868, 12.5f);
    addCity("Gangtok", "Sikkim", 27.3389, 88.6065, 12.5f);

    // Major National Metros & Capitals
    addCity("New Delhi", "National Capital Territory", 28.6139, 77.2090, 12.0f);
    addCity("Mumbai", "Maharashtra", 19.0760, 72.8777, 12.0f);
    addCity("Bengaluru", "Karnataka", 12.9716, 77.5946, 12.0f);
    addCity("Kolkata", "West Bengal", 22.5726, 88.3639, 12.0f);
    addCity("Chennai", "Tamil Nadu", 13.0827, 80.2707, 12.0f);
    addCity("Hyderabad", "Telangana", 17.3850, 78.4867, 12.0f);
    addCity("Ahmedabad", "Gujarat", 23.0225, 72.5714, 12.0f);
    addCity("Pune", "Maharashtra", 18.5204, 73.8567, 12.0f);
    addCity("Jaipur", "Rajasthan", 26.9124, 75.7873, 12.0f);
    addCity("Lucknow", "Uttar Pradesh", 26.8467, 80.9462, 12.0f);
    addCity("Patna", "Bihar", 25.5941, 85.1376, 12.0f);
    addCity("Bhopal", "Madhya Pradesh", 23.2599, 77.4126, 12.0f);
    addCity("Chandigarh", "Punjab & Haryana", 30.7333, 76.7794, 12.0f);
    addCity("Srinagar", "Jammu and Kashmir", 34.0837, 74.7973, 12.0f);
    addCity("Shimla", "Himachal Pradesh", 31.1048, 77.1734, 12.5f);
    addCity("Dehradun", "Uttarakhand", 30.3165, 78.0322, 12.0f);
    addCity("Ranchi", "Jharkhand", 23.3441, 85.3096, 12.0f);
    addCity("Bhubaneswar", "Odisha", 20.2961, 85.8245, 12.0f);
    addCity("Raipur", "Chhattisgarh", 21.2514, 81.6296, 12.0f);
    addCity("Thiruvananthapuram", "Kerala", 8.5241, 76.9366, 12.0f);
    addCity("Kochi", "Kerala", 9.9312, 76.2673, 12.0f);
    addCity("Panaji", "Goa", 15.4909, 73.8278, 12.5f);

    // Major Rivers
    addRiver("Brahmaputra River", "Assam & Northeast India", 26.1800, 91.7500, 10.0f);
    addRiver("Ganga (Ganges) River", "Northern India", 25.3176, 83.0062, 10.0f);
    addRiver("Yamuna River", "Northern India", 28.6139, 77.2300, 10.0f);
    addRiver("Godavari River", "Peninsular India", 16.9891, 81.7840, 10.0f);
    addRiver("Krishna River", "Peninsular India", 16.5100, 80.6400, 10.0f);
    addRiver("Narmada River", "Central India", 21.7000, 73.0000, 10.0f);
    addRiver("Mahanadi River", "Odisha & Chhattisgarh", 20.4625, 85.8830, 10.0f);
    addRiver("Kaveri (Cauvery) River", "Southern India", 10.7905, 79.1378, 10.0f);
}

void SearchBar::setupUi() {
    setFixedWidth(460);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 1. Google Maps Surface Search Card
    cardWidget = new QWidget(this);
    cardWidget->setObjectName("searchCard");
    cardWidget->setStyleSheet(R"(
        QWidget#searchCard {
            background-color: #202124;
            border: 1px solid #3C4043;
            border-radius: 8px;
        }
    )");

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(16);
    shadow->setColor(QColor(0, 0, 0, 120));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);

    auto* cardLayout = new QHBoxLayout(cardWidget);
    cardLayout->setContentsMargins(12, 8, 10, 8);
    cardLayout->setSpacing(10);

    btnSearchIcon = new QPushButton(cardWidget);
    btnSearchIcon->setIcon(IconHelper::search(QColor(138, 180, 248), 18));
    btnSearchIcon->setFlat(true);
    btnSearchIcon->setFixedSize(26, 26);
    btnSearchIcon->setStyleSheet("QPushButton { border: none; background: transparent; }");

    searchInput = new QLineEdit(cardWidget);
    searchInput->setPlaceholderText("Search places, dams, cities (e.g. Hirakud, Guwahati, Delhi)...");
    searchInput->setStyleSheet(R"(
        QLineEdit {
            border: none;
            background: transparent;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 14px;
            font-weight: 500;
            color: #FFFFFF;
            selection-background-color: #8AB4F8;
            selection-color: #202124;
        }
    )");

    btnClear = new QPushButton("✕", cardWidget);
    btnClear->setFlat(true);
    btnClear->setFixedSize(24, 24);
    btnClear->setStyleSheet(R"(
        QPushButton {
            border: none;
            border-radius: 12px;
            font-size: 12px;
            color: #9AA0A6;
            font-weight: bold;
            background-color: transparent;
        }
        QPushButton:hover {
            background-color: #3C4043;
            color: #FFFFFF;
        }
    )");
    btnClear->hide();

    cardLayout->addWidget(btnSearchIcon);
    cardLayout->addWidget(searchInput, 1);
    cardLayout->addWidget(btnClear);

    // 2. Google Search Result List Card (Attached Directly)
    suggestionList = new QListWidget(this);
    suggestionList->setObjectName("suggestionList");
    suggestionList->setStyleSheet(R"(
        QListWidget#suggestionList {
            background-color: #202124;
            border-left: 1px solid #3C4043;
            border-right: 1px solid #3C4043;
            border-bottom: 1px solid #3C4043;
            border-top: 1px solid #303134;
            border-bottom-left-radius: 8px;
            border-bottom-right-radius: 8px;
            outline: none;
            padding: 4px 0px;
        }
        QListWidget#suggestionList::item {
            border: none;
            padding: 0px;
            margin: 0px;
        }
        QListWidget#suggestionList::item:hover {
            background-color: #2D2F33;
        }
        QListWidget#suggestionList::item:selected {
            background-color: #38465C;
        }
    )");
    suggestionList->hide();
    suggestionList->setMaximumHeight(340);
    suggestionList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    mainLayout->addWidget(cardWidget);
    mainLayout->addWidget(suggestionList);

    connect(searchInput, &QLineEdit::textChanged, this, &SearchBar::onTextChanged);
    connect(searchInput, &QLineEdit::returnPressed, this, &SearchBar::onSearchSubmitted);
    connect(btnClear, &QPushButton::clicked, this, &SearchBar::clearSearch);
    connect(suggestionList, &QListWidget::itemClicked, this, &SearchBar::onItemClicked);
}

void SearchBar::updateCardStyle(bool hasResults) {
    if (hasResults && !currentResults.empty()) {
        cardWidget->setStyleSheet(R"(
            QWidget#searchCard {
                background-color: #202124;
                border-top: 1px solid #3C4043;
                border-left: 1px solid #3C4043;
                border-right: 1px solid #3C4043;
                border-bottom: none;
                border-top-left-radius: 8px;
                border-top-right-radius: 8px;
                border-bottom-left-radius: 0px;
                border-bottom-right-radius: 0px;
            }
        )");

        // Dynamic precise height calculation to fit results without clipping or empty space
        int numItems = static_cast<int>(currentResults.size());
        int itemH = 36;
        int listH = std::clamp(numItems * itemH + 6, 36, 320);
        suggestionList->setFixedHeight(listH);
        suggestionList->show();

        cardWidget->adjustSize();
        int totalH = cardWidget->height() + listH;
        setFixedHeight(totalH);
    } else {
        cardWidget->setStyleSheet(R"(
            QWidget#searchCard {
                background-color: #202124;
                border: 1px solid #3C4043;
                border-radius: 8px;
            }
        )");
        suggestionList->hide();
        cardWidget->adjustSize();
        setFixedHeight(cardWidget->height());
    }
    adjustSize();
    emit cardResized();
}

void SearchBar::clearSearch() {
    searchInput->clear();
    btnClear->hide();
    suggestionList->clear();
    suggestionList->hide();
    currentResults.clear();
    updateCardStyle(false);
}

void SearchBar::setQueryText(const QString& text) {
    searchInput->setText(text);
    performSearch(text);
}

void SearchBar::onTextChanged(const QString& text) {
    btnClear->setVisible(!text.trimmed().isEmpty());
    if (text.trimmed().length() >= 1) {
        performSearch(text.trimmed());
        if (debounceTimer) debounceTimer->start();
    } else {
        suggestionList->clear();
        suggestionList->hide();
        currentResults.clear();
        updateCardStyle(false);
    }
}

void SearchBar::performSearch(const QString& query) {
    suggestionList->clear();
    currentResults.clear();

    QString qLower = query.toLower().trimmed();
    if (qLower.isEmpty()) {
        suggestionList->hide();
        updateCardStyle(false);
        return;
    }

    // 1. Search in National Indian Dams Dataset (NRLD / WRIS)
    if (damManager && damManager->hasData()) {
        const auto& allDams = damManager->getDams();
        for (const auto& dam : allDams) {
            QString nameLow = dam.name.toLower();
            QString riverLow = dam.river.toLower();
            QString stateLow = dam.state.toLower();
            QString districtLow = dam.district.toLower();

            bool isMatch = false;

            if (nameLow.startsWith(qLower)) {
                isMatch = true;
            } else if (nameLow.contains(qLower)) {
                isMatch = true;
            } else if (riverLow.startsWith(qLower) || districtLow.startsWith(qLower)) {
                isMatch = true;
            } else if (stateLow.startsWith(qLower)) {
                isMatch = true;
            }

            if (isMatch) {
                SearchResultItem item;
                item.title = dam.name.isEmpty() ? "Unnamed Dam" : dam.name;
                QString detail = QString("Dam & Reservoir · %1, %2")
                    .arg(dam.district.isEmpty() ? dam.river : dam.district)
                    .arg(dam.state.isEmpty() ? "India" : dam.state);
                if (dam.height > 0) detail += QString(" (%1m H)").arg(dam.height, 0, 'f', 0);

                item.subtitle = detail;
                item.iconEmoji = "💧";
                item.lat = dam.lat;
                item.lon = dam.lon;
                item.zoomTarget = 12.0f;
                item.category = MapCore::FeatureCategory::WATER_LAKE;
                item.isDam = true;
                item.damData = dam;

                currentResults.push_back(item);
                if (currentResults.size() >= 12) break;
            }
        }
    }

    // 2. Search in Static India Cities, Metros & Rivers
    for (const auto& loc : staticIndiaLocations) {
        QString tLow = loc.title.toLower();
        QString sLow = loc.subtitle.toLower();

        if (tLow.startsWith(qLower) || tLow.contains(qLower) || sLow.contains(qLower)) {
            currentResults.push_back(loc);
            if (currentResults.size() >= 15) break;
        }
    }

    // 3. Search in Offline OSM Spatial Index (if loaded)
    if (searchIndex && !searchIndex->empty()) {
        for (const auto& item : *searchIndex) {
            QString nLow = QString::fromStdString(item.name).toLower();
            QString dLow = QString::fromStdString(item.detail).toLower();

            if (nLow.startsWith(qLower) || nLow.contains(qLower)) {
                MapCore::GeoCoord geo = MapCore::Projection::mercatorToGeo(item.pos);
                SearchResultItem sItem;
                sItem.title = QString::fromStdString(item.name);
                sItem.subtitle = QString::fromStdString(item.detail);
                sItem.iconEmoji = QString::fromUtf8(MapCore::getCategoryIconEmoji(item.category));
                sItem.lat = geo.lat;
                sItem.lon = geo.lon;
                sItem.zoomTarget = item.zoomTarget;
                sItem.category = item.category;
                sItem.isDam = false;

                currentResults.push_back(sItem);
                if (currentResults.size() >= 18) break;
            }
        }
    }

    // Populate Google-style search items with clean title only
    for (size_t i = 0; i < currentResults.size(); ++i) {
        const auto& res = currentResults[i];

        auto* itemWidget = new QWidget();
        itemWidget->setStyleSheet("background: transparent;");
        auto* rowLayout = new QHBoxLayout(itemWidget);
        rowLayout->setContentsMargins(16, 7, 16, 7);
        rowLayout->setSpacing(0);

        auto* lblTitle = new QLabel(res.title, itemWidget);
        lblTitle->setStyleSheet("color: #FFFFFF; font-family: 'Segoe UI', Arial, sans-serif; font-size: 13px; font-weight: 500;");

        rowLayout->addWidget(lblTitle, 1);

        auto* listItem = new QListWidgetItem(suggestionList);
        listItem->setSizeHint(QSize(440, 36));
        listItem->setData(Qt::UserRole, static_cast<int>(i));

        suggestionList->addItem(listItem);
        suggestionList->setItemWidget(listItem, itemWidget);
    }

    if (!currentResults.empty()) {
        suggestionList->show();
        updateCardStyle(true);
    } else {
        suggestionList->hide();
        updateCardStyle(false);
    }
}

void SearchBar::onDebounceTimeout() {
    QString q = searchInput->text().trimmed();
    if (q.length() < 2) return;

    // Trigger online OSM Nominatim geocoding request in the background
    QUrl url("https://nominatim.openstreetmap.org/search");
    QUrlQuery query;
    query.addQueryItem("q", q);
    query.addQueryItem("format", "json");
    query.addQueryItem("addressdetails", "1");
    query.addQueryItem("limit", "5");
    query.addQueryItem("countrycodes", "in"); // prioritize India
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "AssamMapViewer/2.0 (SIH Project)");
    if (networkManager) {
        networkManager->get(request);
    }
}

void SearchBar::onGeocodeReply(QNetworkReply* reply) {
    if (!reply) return;
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) return;

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) return;

    QJsonArray array = doc.array();
    if (array.isEmpty()) return;

    QString currentQuery = searchInput->text().trimmed().toLower();
    if (currentQuery.length() < 2) return;

    bool addedAny = false;

    for (const auto& val : array) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        QString dispName = obj.value("display_name").toString();
        QString type = obj.value("type").toString();
        double lat = obj.value("lat").toString().toDouble();
        double lon = obj.value("lon").toString().toDouble();

        if (std::abs(lat) < 0.001 && std::abs(lon) < 0.001) continue;

        QString shortTitle = dispName.section(',', 0, 0).trimmed();
        QString shortSub = dispName.section(',', 1, 3).trimmed();

        // Check if already in current results
        bool alreadyExists = false;
        for (const auto& r : currentResults) {
            if (std::abs(r.lat - lat) < 0.01 && std::abs(r.lon - lon) < 0.01) {
                alreadyExists = true;
                break;
            }
        }

        if (alreadyExists) continue;

        SearchResultItem item;
        item.title = shortTitle;
        item.subtitle = shortSub.isEmpty() ? "Location · India" : shortSub;
        item.iconEmoji = (type == "city" || type == "administrative") ? "🏙️" : "📍";
        item.lat = lat;
        item.lon = lon;
        item.zoomTarget = 12.0f;
        item.category = MapCore::FeatureCategory::PLACE_CITY;
        item.isDam = false;

        currentResults.push_back(item);
        addedAny = true;
    }

    if (addedAny && !currentResults.empty()) {
        // Re-populate list items
        suggestionList->clear();
        for (size_t i = 0; i < currentResults.size(); ++i) {
            const auto& res = currentResults[i];

            auto* itemWidget = new QWidget();
            itemWidget->setStyleSheet("background: transparent;");
            auto* rowLayout = new QHBoxLayout(itemWidget);
            rowLayout->setContentsMargins(16, 7, 16, 7);
            rowLayout->setSpacing(0);

            auto* lblTitle = new QLabel(res.title, itemWidget);
            lblTitle->setStyleSheet("color: #FFFFFF; font-family: 'Segoe UI', Arial, sans-serif; font-size: 13px; font-weight: 500;");

            rowLayout->addWidget(lblTitle, 1);

            auto* listItem = new QListWidgetItem(suggestionList);
            listItem->setSizeHint(QSize(440, 36));
            listItem->setData(Qt::UserRole, static_cast<int>(i));

            suggestionList->addItem(listItem);
            suggestionList->setItemWidget(listItem, itemWidget);
        }

        suggestionList->show();
        updateCardStyle(true);
    }
}

void SearchBar::onItemClicked(QListWidgetItem* item) {
    int idx = item->data(Qt::UserRole).toInt();
    if (idx >= 0 && idx < static_cast<int>(currentResults.size())) {
        const auto& match = currentResults[idx];
        suggestionList->hide();
        updateCardStyle(false);
        searchInput->setText(match.title);

        if (match.isDam) {
            emit damResultSelected(match.damData);
        } else {
            MapCore::GeoCoord geo(match.lat, match.lon);
            MapCore::Point2D merc = MapCore::Projection::geoToMercator(geo);
            emit searchResultSelected(merc, match.zoomTarget, match.title, match.subtitle, match.category);
        }
    }
}

void SearchBar::onSearchSubmitted() {
    if (!currentResults.empty()) {
        const auto& match = currentResults.front();
        suggestionList->hide();
        updateCardStyle(false);
        searchInput->setText(match.title);

        if (match.isDam) {
            emit damResultSelected(match.damData);
        } else {
            MapCore::GeoCoord geo(match.lat, match.lon);
            MapCore::Point2D merc = MapCore::Projection::geoToMercator(geo);
            emit searchResultSelected(merc, match.zoomTarget, match.title, match.subtitle, match.category);
        }
    }
}

} // namespace MapUI
