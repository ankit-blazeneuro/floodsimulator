#include "SearchBar.h"
#include "IconHelper.h"
#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QKeyEvent>
#include <algorithm>

namespace MapUI {

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void SearchBar::setSearchIndex(const std::vector<MapCore::SearchItem>& index) {
    searchIndex = &index;
}

void SearchBar::setupUi() {
    setFixedWidth(440);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(6);

    // 1. Search Bar Card (Dark Google Maps Surface)
    auto* cardWidget = new QWidget(this);
    cardWidget->setObjectName("searchCard");
    cardWidget->setStyleSheet(R"(
        QWidget#searchCard {
            background-color: #202124;
            border: 1px solid #3C4043;
            border-radius: 8px;
        }
    )");

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(14);
    shadow->setColor(QColor(0, 0, 0, 90));
    shadow->setOffset(0, 4);
    cardWidget->setGraphicsEffect(shadow);

    auto* cardLayout = new QHBoxLayout(cardWidget);
    cardLayout->setContentsMargins(12, 6, 8, 6);
    cardLayout->setSpacing(8);

    btnSearchIcon = new QPushButton(cardWidget);
    btnSearchIcon->setIcon(IconHelper::zoomIn(QColor(138, 180, 248), 18));
    btnSearchIcon->setFlat(true);
    btnSearchIcon->setStyleSheet("QPushButton { border: none; }");

    searchInput = new QLineEdit(cardWidget);
    searchInput->setPlaceholderText("Search places, cities, highways (e.g. Guwahati, Delhi, NH 27)...");
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
            color: #FFFFFF;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #3C4043;
            color: #8AB4F8;
        }
    )");
    btnClear->hide();

    cardLayout->addWidget(btnSearchIcon);
    cardLayout->addWidget(searchInput);
    cardLayout->addWidget(btnClear);

    // 2. Filter Chips (Dark High-Contrast Pills)
    chipsWidget = new QWidget(this);
    auto* chipsLayout = new QHBoxLayout(chipsWidget);
    chipsLayout->setContentsMargins(2, 0, 2, 0);
    chipsLayout->setSpacing(6);

    QString chipStyle = R"(
        QPushButton {
            background-color: #2D3135;
            color: #FFFFFF;
            border: 1px solid #5F6368;
            border-radius: 14px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 11px;
            font-weight: bold;
            padding: 5px 12px;
        }
        QPushButton:hover {
            background-color: #3C4043;
            color: #8AB4F8;
            border-color: #8AB4F8;
        }
        QPushButton:pressed {
            background-color: #1A1C1E;
        }
    )";

    auto makeChip = [&](const QString& label, MapCore::FeatureCategory cat, const QString& searchQ = "") {
        auto* chip = new QPushButton(label, chipsWidget);
        chip->setStyleSheet(chipStyle);
        connect(chip, &QPushButton::clicked, this, [this, cat, searchQ]() {
            if (!searchQ.isEmpty()) {
                searchInput->setText(searchQ);
                performSearch(searchQ);
            } else {
                emit categoryFilterClicked(cat);
            }
        });
        chipsLayout->addWidget(chip);
    };

    makeChip("Guwahati", MapCore::FeatureCategory::PLACE_CITY, "Guwahati");
    makeChip("Dibrugarh", MapCore::FeatureCategory::PLACE_CITY, "Dibrugarh");
    makeChip("Highways", MapCore::FeatureCategory::HIGHWAY_TRUNK, "NH");
    makeChip("Rivers", MapCore::FeatureCategory::WATER_RIVER, "Brahmaputra");
    makeChip("Hospitals", MapCore::FeatureCategory::POI_HOSPITAL, "Hospital");

    // 3. Dropdown Suggestion List (Dark Theme)
    suggestionList = new QListWidget(this);
    suggestionList->setObjectName("suggestionList");
    suggestionList->setStyleSheet(R"(
        QListWidget#suggestionList {
            background-color: #202124;
            border: 1px solid #3C4043;
            border-radius: 8px;
            font-family: 'Segoe UI', Arial, sans-serif;
            font-size: 13px;
            color: #E8EAED;
            padding: 4px;
        }
        QListWidget#suggestionList::item {
            padding: 8px 10px;
            border-radius: 4px;
            color: #E8EAED;
        }
        QListWidget#suggestionList::item:hover {
            background-color: #303134;
        }
        QListWidget#suggestionList::item:selected {
            background-color: #38465C;
            color: #8AB4F8;
        }
    )");
    suggestionList->hide();
    suggestionList->setMaximumHeight(260);

    auto* sListShadow = new QGraphicsDropShadowEffect(this);
    sListShadow->setBlurRadius(14);
    sListShadow->setColor(QColor(0, 0, 0, 90));
    sListShadow->setOffset(0, 4);
    suggestionList->setGraphicsEffect(sListShadow);

    mainLayout->addWidget(cardWidget);
    mainLayout->addWidget(chipsWidget);
    mainLayout->addWidget(suggestionList);

    connect(searchInput, &QLineEdit::textChanged, this, &SearchBar::onTextChanged);
    connect(searchInput, &QLineEdit::returnPressed, this, &SearchBar::onSearchSubmitted);
    connect(btnClear, &QPushButton::clicked, this, &SearchBar::clearSearch);
    connect(suggestionList, &QListWidget::itemClicked, this, &SearchBar::onItemClicked);
}

void SearchBar::clearSearch() {
    searchInput->clear();
    btnClear->hide();
    suggestionList->clear();
    suggestionList->hide();
    currentMatches.clear();
}

void SearchBar::setQueryText(const QString& text) {
    searchInput->setText(text);
    performSearch(text);
}

void SearchBar::onTextChanged(const QString& text) {
    btnClear->setVisible(!text.trimmed().isEmpty());
    if (text.trimmed().length() >= 1) {
        performSearch(text.trimmed());
    } else {
        suggestionList->clear();
        suggestionList->hide();
        currentMatches.clear();
    }
}

void SearchBar::performSearch(const QString& query) {
    if (!searchIndex || searchIndex->empty()) return;

    suggestionList->clear();
    currentMatches.clear();

    QString qLower = query.toLower();

    std::vector<std::pair<int, const MapCore::SearchItem*>> matches;

    for (const auto& item : *searchIndex) {
        QString nameLower = QString::fromStdString(item.name).toLower();
        QString detailLower = QString::fromStdString(item.detail).toLower();

        if (nameLower.startsWith(qLower)) {
            matches.push_back({ item.priority * 10, &item });
        } else if (nameLower.contains(qLower)) {
            matches.push_back({ item.priority * 10 + 50, &item });
        } else if (detailLower.contains(qLower)) {
            matches.push_back({ item.priority * 10 + 100, &item });
        }

        if (matches.size() >= 80) break;
    }

    std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });

    int count = 0;
    for (const auto& m : matches) {
        const auto* item = m.second;
        currentMatches.push_back(*item);

        QString icon = QString::fromUtf8(MapCore::getCategoryIconEmoji(item->category));
        QString itemText = QString("%1 %2\n   (%3)")
            .arg(icon)
            .arg(QString::fromStdString(item->name))
            .arg(QString::fromStdString(item->detail));

        auto* listItem = new QListWidgetItem(itemText, suggestionList);
        listItem->setData(Qt::UserRole, count);
        count++;

        if (count >= 10) break;
    }

    if (!currentMatches.empty()) {
        suggestionList->show();
    } else {
        suggestionList->hide();
    }
}

void SearchBar::onItemClicked(QListWidgetItem* item) {
    int idx = item->data(Qt::UserRole).toInt();
    if (idx >= 0 && idx < (int)currentMatches.size()) {
        const auto& match = currentMatches[idx];
        suggestionList->hide();
        searchInput->setText(QString::fromStdString(match.name));

        emit searchResultSelected(match.pos, match.zoomTarget,
                                  QString::fromStdString(match.name),
                                  QString::fromStdString(match.detail),
                                  match.category);
    }
}

void SearchBar::onSearchSubmitted() {
    if (!currentMatches.empty()) {
        const auto& match = currentMatches.front();
        suggestionList->hide();
        searchInput->setText(QString::fromStdString(match.name));

        emit searchResultSelected(match.pos, match.zoomTarget,
                                  QString::fromStdString(match.name),
                                  QString::fromStdString(match.detail),
                                  match.category);
    }
}

} // namespace MapUI
