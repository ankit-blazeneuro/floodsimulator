#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <iostream>
#include "src/core/SpatialIndex.h"
#include "src/core/MapDataCache.h"
#include "src/renderer/MapRenderer.h"

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);

    MapCore::SpatialIndex index;
    if (!MapCore::MapDataCache::loadCache("data/assam_map.bin", index)) {
        std::cerr << "Failed to load cache!\n";
        return 1;
    }

    int width = 1280;
    int height = 800;

    MapRenderer::MapRenderer renderer(MapRenderer::ThemePreset::GOOGLE_DARK);

    // Guwahati center (Lat 26.18, Lon 91.75)
    {
        QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
        MapCore::Point2D guwahatiCenter = MapCore::Projection::geoToMercator(MapCore::GeoCoord(26.18, 91.75));
        float zoomLevel = 12.0f; // City level

        QPainter painter(&image);
        renderer.render(painter, index, guwahatiCenter, zoomLevel, width, height);
        painter.end();

        image.save("map_dark_guwahati.png");
        std::cout << "Saved map_dark_guwahati.png successfully!\n";
    }

    // Assam overview zoom
    {
        QImage imageOverview(width, height, QImage::Format_ARGB32_Premultiplied);
        QPainter painter2(&imageOverview);
        MapCore::Point2D assamCenter = MapCore::Projection::geoToMercator(MapCore::GeoCoord(26.2, 92.8));
        renderer.render(painter2, index, assamCenter, 8.2f, width, height);
        painter2.end();

        imageOverview.save("map_dark_assam.png");
        std::cout << "Saved map_dark_assam.png successfully!\n";
    }

    return 0;
}
