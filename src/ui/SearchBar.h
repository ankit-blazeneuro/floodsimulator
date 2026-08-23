#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <vector>
#include "../core/SpatialIndex.h"

namespace MapUI {

class SearchBar : public QWidget {
    Q_OBJECT

private:
    QLineEdit* searchInput;
    QPushButton* btnClear;
    QPushButton* btnSearchIcon;
    QListWidget* suggestionList;
    QWidget* chipsWidget;

    const std::vector<MapCore::SearchItem>* searchIndex = nullptr;
    std::vector<MapCore::SearchItem> currentMatches;

public:
    explicit SearchBar(QWidget* parent = nullptr);

    void setSearchIndex(const std::vector<MapCore::SearchItem>& index);

    void clearSearch();
    void setQueryText(const QString& text);

signals:
    void searchResultSelected(MapCore::Point2D pos, float targetZoom,
                              QString name, QString detail, MapCore::FeatureCategory category);
    void categoryFilterClicked(MapCore::FeatureCategory category);

private slots:
    void onTextChanged(const QString& text);
    void onItemClicked(QListWidgetItem* item);
    void onSearchSubmitted();

private:
    void setupUi();
    void performSearch(const QString& query);
};

} // namespace MapUI
