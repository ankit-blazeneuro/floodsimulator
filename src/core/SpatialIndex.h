#pragma once

#include "GeoTypes.h"
#include "MapFeature.h"
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>
#include <string>

namespace MapCore {

struct FeatureInfo {
    bool found = false;
    FeatureCategory category = FeatureCategory::UNKNOWN;
    std::string name;
    std::string detail;
    GeoCoord geoCoord;
    Point2D mercatorPos;
    float distanceMeters = 0.0f;
    std::vector<std::pair<std::string, std::string>> tags;
};

class SpatialIndex {
public:
    static constexpr int GRID_COLS = 128;
    static constexpr int GRID_ROWS = 128;

    struct GridCell {
        std::vector<uint32_t> polylineIndices;
        std::vector<uint32_t> polygonIndices;
        std::vector<uint32_t> pointIndices;
    };

    // Primary data storage
    std::vector<MapPolygon> polygons;
    std::vector<MapPolyline> polylines;
    std::vector<MapPoint> points;
    std::vector<SearchItem> searchItems;

    BoundingBox extent;

    // Grid cells
    std::vector<GridCell> grid;

    // Fast deduplication frame tracker
    mutable uint32_t frameId = 0;
    mutable std::vector<uint32_t> polygonVisited;
    mutable std::vector<uint32_t> polylineVisited;
    mutable std::vector<uint32_t> pointVisited;

public:
    SpatialIndex() {
        grid.resize(GRID_COLS * GRID_ROWS);
    }

    void clear() {
        polygons.clear();
        polylines.clear();
        points.clear();
        searchItems.clear();
        extent = BoundingBox();
        for (auto& cell : grid) {
            cell.polylineIndices.clear();
            cell.polygonIndices.clear();
            cell.pointIndices.clear();
        }
        polygonVisited.clear();
        polylineVisited.clear();
        pointVisited.clear();
        frameId = 0;
    }

    void buildIndex() {
        grid.assign(GRID_COLS * GRID_ROWS, GridCell());

        if (!extent.isValid()) {
            extent = BoundingBox(0.74f, 0.41f, 0.77f, 0.45f); // default Assam extent
        }

        // Add small margin to extent
        float marginX = extent.width() * 0.02f;
        float marginY = extent.height() * 0.02f;
        BoundingBox indexBounds(extent.minX - marginX, extent.minY - marginY,
                                extent.maxX + marginX, extent.maxY + marginY);
        extent = indexBounds;

        // Index Polygons
        for (uint32_t i = 0; i < polygons.size(); ++i) {
            const auto& poly = polygons[i];
            int minCol, maxCol, minRow, maxRow;
            getGridRange(poly.bbox, minCol, maxCol, minRow, maxRow);
            for (int r = minRow; r <= maxRow; ++r) {
                for (int c = minCol; c <= maxCol; ++c) {
                    grid[r * GRID_COLS + c].polygonIndices.push_back(i);
                }
            }
        }

        // Index Polylines
        for (uint32_t i = 0; i < polylines.size(); ++i) {
            const auto& line = polylines[i];
            int minCol, maxCol, minRow, maxRow;
            getGridRange(line.bbox, minCol, maxCol, minRow, maxRow);
            for (int r = minRow; r <= maxRow; ++r) {
                for (int c = minCol; c <= maxCol; ++c) {
                    grid[r * GRID_COLS + c].polylineIndices.push_back(i);
                }
            }
        }

        // Index Points
        for (uint32_t i = 0; i < points.size(); ++i) {
            const auto& pt = points[i];
            int c = getCol(pt.pos.x);
            int r = getRow(pt.pos.y);
            if (c >= 0 && c < GRID_COLS && r >= 0 && r < GRID_ROWS) {
                grid[r * GRID_COLS + c].pointIndices.push_back(i);
            }
        }

        polygonVisited.assign(polygons.size(), 0);
        polylineVisited.assign(polylines.size(), 0);
        pointVisited.assign(points.size(), 0);
    }

    void queryViewport(const BoundingBox& viewBbox, float zoomLevel,
                       std::vector<const MapPolygon*>& outPolygons,
                       std::vector<const MapPolyline*>& outPolylines,
                       std::vector<const MapPoint*>& outPoints) const {
        frameId++;
        if (frameId == 0) { // wrap around overflow protection
            polygonVisited.assign(polygons.size(), 0);
            polylineVisited.assign(polylines.size(), 0);
            pointVisited.assign(points.size(), 0);
            frameId = 1;
        }

        int minCol, maxCol, minRow, maxRow;
        getGridRange(viewBbox, minCol, maxCol, minRow, maxRow);

        for (int r = minRow; r <= maxRow; ++r) {
            for (int c = minCol; c <= maxCol; ++c) {
                const auto& cell = grid[r * GRID_COLS + c];

                // Polygons
                for (uint32_t idx : cell.polygonIndices) {
                    if (polygonVisited[idx] != frameId) {
                        polygonVisited[idx] = frameId;
                        const auto& poly = polygons[idx];
                        if (zoomLevel >= poly.minZoom && zoomLevel <= poly.maxZoom && poly.bbox.intersects(viewBbox)) {
                            outPolygons.push_back(&poly);
                        }
                    }
                }

                // Polylines
                for (uint32_t idx : cell.polylineIndices) {
                    if (polylineVisited[idx] != frameId) {
                        polylineVisited[idx] = frameId;
                        const auto& line = polylines[idx];
                        if (zoomLevel >= line.minZoom && zoomLevel <= line.maxZoom && line.bbox.intersects(viewBbox)) {
                            outPolylines.push_back(&line);
                        }
                    }
                }

                // Points
                for (uint32_t idx : cell.pointIndices) {
                    if (pointVisited[idx] != frameId) {
                        pointVisited[idx] = frameId;
                        const auto& pt = points[idx];
                        if (zoomLevel >= pt.minZoom && zoomLevel <= pt.maxZoom && viewBbox.contains(pt.pos)) {
                            outPoints.push_back(&pt);
                        }
                    }
                }
            }
        }
    }

    FeatureInfo findNearest(Point2D pos, float maxMercatorRadius, float zoomLevel) const {
        FeatureInfo result;
        float bestDistSq = maxMercatorRadius * maxMercatorRadius;

        BoundingBox searchBbox(pos.x - maxMercatorRadius, pos.y - maxMercatorRadius,
                              pos.x + maxMercatorRadius, pos.y + maxMercatorRadius);

        int minCol, maxCol, minRow, maxRow;
        getGridRange(searchBbox, minCol, maxCol, minRow, maxRow);

        // First check Points (Cities, POIs)
        for (int r = minRow; r <= maxRow; ++r) {
            for (int c = minCol; c <= maxCol; ++c) {
                const auto& cell = grid[r * GRID_COLS + c];
                for (uint32_t idx : cell.pointIndices) {
                    const auto& pt = points[idx];
                    if (zoomLevel < pt.minZoom) continue;
                    float dSq = pos.distanceSquaredTo(pt.pos);
                    if (dSq < bestDistSq) {
                        bestDistSq = dSq;
                        result.found = true;
                        result.category = pt.category;
                        result.name = pt.name;
                        result.detail = pt.categoryLabel;
                        result.mercatorPos = pt.pos;
                        result.geoCoord = Projection::mercatorToGeo(pt.pos);
                        result.tags = pt.tags;
                    }
                }
            }
        }

        // Then check Polylines (Roads, Rivers)
        for (int r = minRow; r <= maxRow; ++r) {
            for (int c = minCol; c <= maxCol; ++c) {
                const auto& cell = grid[r * GRID_COLS + c];
                for (uint32_t idx : cell.polylineIndices) {
                    const auto& line = polylines[idx];
                    if (zoomLevel < line.minZoom) continue;
                    if (!line.bbox.intersects(searchBbox)) continue;

                    const auto& pts = (zoomLevel <= 11.0f && !line.lodPoints.empty()) ? line.lodPoints : line.points;
                    for (size_t i = 0; i + 1 < pts.size(); ++i) {
                        float dSq = GeometryUtils::perpendicularDistanceSq(pos, pts[i], pts[i+1]);
                        if (dSq < bestDistSq) {
                            bestDistSq = dSq;
                            result.found = true;
                            result.category = line.category;
                            result.name = !line.name.empty() ? line.name : (!line.ref.empty() ? line.ref : getCategoryDisplayName(line.category));
                            result.detail = !line.ref.empty() ? ("Highway " + line.ref) : getCategoryDisplayName(line.category);
                            result.mercatorPos = pos;
                            result.geoCoord = Projection::mercatorToGeo(pos);
                            result.tags = line.tags;
                        }
                    }
                }
            }
        }

        // Then check Polygons (Buildings, Parks, Lakes) if zoom is high
        if (zoomLevel >= 13.0f) {
            for (int r = minRow; r <= maxRow; ++r) {
                for (int c = minCol; c <= maxCol; ++c) {
                    const auto& cell = grid[r * GRID_COLS + c];
                    for (uint32_t idx : cell.polygonIndices) {
                        const auto& poly = polygons[idx];
                        if (zoomLevel < poly.minZoom) continue;
                        if (!poly.bbox.contains(pos)) continue;

                        result.found = true;
                        result.category = poly.category;
                        result.name = !poly.name.empty() ? poly.name : getCategoryDisplayName(poly.category);
                        result.detail = getCategoryDisplayName(poly.category);
                        result.mercatorPos = pos;
                        result.geoCoord = Projection::mercatorToGeo(pos);
                        result.tags = poly.tags;
                        break;
                    }
                }
            }
        }

        if (result.found) {
            result.distanceMeters = static_cast<float>(Projection::mercatorDistToMeters(std::sqrt(bestDistSq), result.geoCoord.lat));
        }

        return result;
    }

private:
    int getCol(float x) const {
        if (extent.width() <= 0.0f) return 0;
        int col = static_cast<int>((x - extent.minX) / extent.width() * GRID_COLS);
        return std::clamp(col, 0, GRID_COLS - 1);
    }

    int getRow(float y) const {
        if (extent.height() <= 0.0f) return 0;
        int row = static_cast<int>((y - extent.minY) / extent.height() * GRID_ROWS);
        return std::clamp(row, 0, GRID_ROWS - 1);
    }

    void getGridRange(const BoundingBox& box, int& minCol, int& maxCol, int& minRow, int& maxRow) const {
        minCol = getCol(box.minX);
        maxCol = getCol(box.maxX);
        minRow = getRow(box.minY);
        maxRow = getRow(box.maxY);
    }
};

} // namespace MapCore
