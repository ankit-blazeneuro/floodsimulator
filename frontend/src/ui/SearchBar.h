#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <vector>
#include "../core/SpatialIndex.h"
#include "../core/DamManager.h"

namespace MapUI {

struct SearchResultItem {
    QString title;
    QString subtitle;
    QString iconEmoji;
    double lat = 0.0;
    double lon = 0.0;
    float zoomTarget = 12.0f;
    MapCore::FeatureCategory category = MapCore::FeatureCategory::PLACE_CITY;
    bool isDam = false;
    MapCore::DamPoint damData;
};

class SearchBar : public QWidget {
    Q_OBJECT

private:
    QWidget* cardWidget;
    QLineEdit* searchInput;
    QPushButton* btnClear;
    QPushButton* btnSearchIcon;
    QLabel* lblKbdBadge;
    QListWidget* suggestionList;

    const std::vector<MapCore::SearchItem>* searchIndex = nullptr;
    const MapCore::DamManager* damManager = nullptr;
    std::vector<SearchResultItem> currentResults;

    QNetworkAccessManager* networkManager = nullptr;
    QTimer* debounceTimer = nullptr;

    void initStaticLocations();
    std::vector<SearchResultItem> staticIndiaLocations;

    QWidget* createItemWidget(const SearchResultItem& res);

public:
    explicit SearchBar(QWidget* parent = nullptr);

    void setSearchIndex(const std::vector<MapCore::SearchItem>& index);
    void setDamManager(const MapCore::DamManager* mgr);

    void clearSearch();
    void setQueryText(const QString& text);
    void focusInput();

signals:
    void searchResultSelected(MapCore::Point2D pos, float targetZoom,
                              QString name, QString detail, MapCore::FeatureCategory category);
    void damResultSelected(const MapCore::DamPoint& dam);
    void categoryFilterClicked(MapCore::FeatureCategory category);
    void cardResized();

private slots:
    void onTextChanged(const QString& text);
    void onItemClicked(QListWidgetItem* item);
    void onSearchSubmitted();
    void onDebounceTimeout();
    void onGeocodeReply(QNetworkReply* reply);

private:
    void setupUi();
    void performSearch(const QString& query);
    void updateCardStyle(bool hasResults);
    void populateList();
};

} // namespace MapUI
