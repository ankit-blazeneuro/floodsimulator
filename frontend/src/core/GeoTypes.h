#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdint>

namespace MapCore {

// Constants
constexpr double PI = 3.14159265358979323846;
constexpr double DEG_TO_RAD = PI / 180.0;
constexpr double RAD_TO_DEG = 180.0 / PI;
constexpr double EARTH_RADIUS_METERS = 6378137.0;

// Geographic coordinate (WGS84 Lat/Lon in degrees)
struct GeoCoord {
    double lat = 0.0;
    double lon = 0.0;

    GeoCoord() = default;
    GeoCoord(double latitude, double longitude) : lat(latitude), lon(longitude) {}

    bool isValid() const {
        return lat >= -85.05112878 && lat <= 85.05112878 && lon >= -180.0 && lon <= 180.0;
    }
};

// 2D Point in normalized Web Mercator coordinates [0.0, 1.0] x [0.0, 1.0]
// (0,0) is top-left (North-West), (1,1) is bottom-right (South-East)
struct Point2D {
    float x = 0.0f;
    float y = 0.0f;

    Point2D() = default;
    Point2D(float px, float py) : x(px), y(py) {}

    bool operator==(const Point2D& other) const {
        return std::abs(x - other.x) < 1e-7f && std::abs(y - other.y) < 1e-7f;
    }

    float distanceTo(const Point2D& o) const {
        float dx = x - o.x;
        float dy = y - o.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    float distanceSquaredTo(const Point2D& o) const {
        float dx = x - o.x;
        float dy = y - o.y;
        return dx * dx + dy * dy;
    }
};

// Bounding Box in normalized Web Mercator coordinates
struct BoundingBox {
    float minX = 1.0f;
    float minY = 1.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    BoundingBox() = default;
    BoundingBox(float x1, float y1, float x2, float y2)
        : minX(std::min(x1, x2)), minY(std::min(y1, y2)),
          maxX(std::max(x1, x2)), maxY(std::max(y1, y2)) {}

    bool isValid() const {
        return minX <= maxX && minY <= maxY;
    }

    void expand(const Point2D& pt) {
        if (pt.x < minX) minX = pt.x;
        if (pt.x > maxX) maxX = pt.x;
        if (pt.y < minY) minY = pt.y;
        if (pt.y > maxY) maxY = pt.y;
    }

    void expand(const BoundingBox& other) {
        if (!other.isValid()) return;
        if (other.minX < minX) minX = other.minX;
        if (other.maxX > maxX) maxX = other.maxX;
        if (other.minY < minY) minY = other.minY;
        if (other.maxY > maxY) maxY = other.maxY;
    }

    bool intersects(const BoundingBox& other) const {
        return !(maxX < other.minX || minX > other.maxX ||
                 maxY < other.minY || minY > other.maxY);
    }

    bool contains(const Point2D& pt) const {
        return pt.x >= minX && pt.x <= maxX && pt.y >= minY && pt.y <= maxY;
    }

    Point2D center() const {
        return Point2D((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    }

    float width() const { return maxX - minX; }
    float height() const { return maxY - minY; }
};

// Web Mercator projection conversions
class Projection {
public:
    // Convert WGS84 (Lat, Lon) to normalized Mercator [0, 1]
    static inline Point2D geoToMercator(const GeoCoord& geo) {
        double lon = std::clamp(geo.lon, -180.0, 180.0);
        double lat = std::clamp(geo.lat, -85.05112878, 85.05112878);

        double x = (lon + 180.0) / 360.0;
        double sinLat = std::sin(lat * DEG_TO_RAD);
        double y = (0.5 - std::log((1.0 + sinLat) / (1.0 - sinLat)) / (4.0 * PI));

        return Point2D(static_cast<float>(x), static_cast<float>(y));
    }

    // Convert normalized Mercator [0, 1] to WGS84 (Lat, Lon)
    static inline GeoCoord mercatorToGeo(const Point2D& pt) {
        double lon = pt.x * 360.0 - 180.0;
        double y = std::clamp(static_cast<double>(pt.y), 0.0, 1.0);
        double n = PI - 2.0 * PI * y;
        double lat = RAD_TO_DEG * std::atan(0.5 * (std::exp(n) - std::exp(-n)));

        return GeoCoord(lat, lon);
    }

    // Great circle distance in meters (Haversine formula)
    static inline double haversineDistanceMeters(const GeoCoord& a, const GeoCoord& b) {
        double dLat = (b.lat - a.lat) * DEG_TO_RAD;
        double dLon = (b.lon - a.lon) * DEG_TO_RAD;
        double lat1 = a.lat * DEG_TO_RAD;
        double lat2 = b.lat * DEG_TO_RAD;

        double sinDLat = std::sin(dLat * 0.5);
        double sinDLon = std::sin(dLon * 0.5);

        double h = sinDLat * sinDLat + std::cos(lat1) * std::cos(lat2) * sinDLon * sinDLon;
        double c = 2.0 * std::atan2(std::sqrt(h), std::sqrt(1.0 - h));
        return EARTH_RADIUS_METERS * c;
    }

    // Convert Mercator distance to approximate ground distance in meters at a given latitude
    static inline double mercatorDistToMeters(float mercatorDist, double lat) {
        double cosLat = std::cos(lat * DEG_TO_RAD);
        double circumferenceAtLat = 2.0 * PI * EARTH_RADIUS_METERS * cosLat;
        return mercatorDist * circumferenceAtLat;
    }
};

// Fast Ramer-Douglas-Peucker Polyline Simplification for Level-of-Detail (LOD)
class GeometryUtils {
public:
    static float perpendicularDistanceSq(const Point2D& pt, const Point2D& lineStart, const Point2D& lineEnd) {
        float dx = lineEnd.x - lineStart.x;
        float dy = lineEnd.y - lineStart.y;
        float lenSq = dx * dx + dy * dy;

        if (lenSq < 1e-12f) {
            return pt.distanceSquaredTo(lineStart);
        }

        float t = ((pt.x - lineStart.x) * dx + (pt.y - lineStart.y) * dy) / lenSq;
        t = std::clamp(t, 0.0f, 1.0f);

        Point2D proj(lineStart.x + t * dx, lineStart.y + t * dy);
        return pt.distanceSquaredTo(proj);
    }

    static void simplifyRDPRecursive(const std::vector<Point2D>& points, size_t startIdx, size_t endIdx,
                                     float epsilonSq, std::vector<bool>& keep) {
        if (endIdx <= startIdx + 1) return;

        float maxDistSq = 0.0f;
        size_t maxIdx = startIdx;

        for (size_t i = startIdx + 1; i < endIdx; ++i) {
            float distSq = perpendicularDistanceSq(points[i], points[startIdx], points[endIdx]);
            if (distSq > maxDistSq) {
                maxDistSq = distSq;
                maxIdx = i;
            }
        }

        if (maxDistSq > epsilonSq) {
            keep[maxIdx] = true;
            simplifyRDPRecursive(points, startIdx, maxIdx, epsilonSq, keep);
            simplifyRDPRecursive(points, maxIdx, endIdx, epsilonSq, keep);
        }
    }

    static std::vector<Point2D> simplifyPolyline(const std::vector<Point2D>& points, float epsilon) {
        if (points.size() <= 2) return points;

        float epsilonSq = epsilon * epsilon;
        std::vector<bool> keep(points.size(), false);
        keep.front() = true;
        keep.back() = true;

        simplifyRDPRecursive(points, 0, points.size() - 1, epsilonSq, keep);

        std::vector<Point2D> result;
        result.reserve(points.size());
        for (size_t i = 0; i < points.size(); ++i) {
            if (keep[i]) {
                result.push_back(points[i]);
            }
        }
        return result;
    }
};

} // namespace MapCore
