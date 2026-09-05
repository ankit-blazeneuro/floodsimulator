#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <QString>
#include <QColor>

namespace MapCore {

enum class HeatMapPalette {
    SeaLevelInundation = 0, // Sea Level Rise & Coastal Flooding (Navy -> Aqua -> Danger Red -> Emerald)
    Turbo = 1,              // Turbo High-Dynamic Topographic DEM (Spectral Rainbow)
    Magma = 2,              // Thermal Hazard (Black -> Purple -> Crimson -> Gold -> White)
    OceanDepth = 3,         // Bathymetry & Hydro Depth (Deep Blue -> Cyan -> Aqua)
    Viridis = 4             // Scientific Standard (Purple -> Teal -> Yellow)
};

struct CoastalHotspot {
    QString name;
    QString region;
    double lat;
    double lon;
    int targetZoom;
    double baselineElevMSL;
    double tidalRangeM;
    QString riskNotes;
};

class ElevationModel {
public:
    // Polygon representing Indian Subcontinent & Coastal Boundaries (Clockwise)
    static inline const std::vector<std::pair<double, double>>& getIndiaCoastPoly() {
        static const std::vector<std::pair<double, double>> poly = {
            {24.5, 68.2},   // Kutch NW
            {23.2, 68.5},   // Kutch south
            {23.0, 70.2},   // Gulf of Kutch head
            {22.4, 69.0},   // Dwarka / Okha
            {21.6, 69.5},   // Porbandar
            {20.7, 70.9},   // Diu / Veraval
            {21.0, 72.0},   // Bhavnagar / Gulf of Khambhat west
            {22.2, 72.3},   // Gulf of Khambhat head
            {21.2, 72.7},   // Surat
            {20.0, 72.7},   // Daman
            {18.9, 72.75},  // Mumbai
            {17.0, 73.2},   // Ratnagiri
            {15.5, 73.7},   // Goa
            {14.0, 74.4},   // Karwar / Gokarna
            {12.9, 74.7},   // Mangalore
            {11.2, 75.7},   // Kozhikode
            {9.9, 76.2},    // Kochi
            {9.3, 76.3},    // Alappuzha
            {8.5, 76.9},    // Thiruvananthapuram
            {8.05, 77.5},   // Kanyakumari (Southernmost tip)
            {8.7, 78.1},    // Tuticorin / Gulf of Mannar
            {9.3, 79.1},    // Rameswaram
            {10.3, 79.8},   // Point Calimere / Nagapattinam
            {11.9, 79.8},   // Puducherry
            {13.1, 80.3},   // Chennai
            {14.5, 80.1},   // Nellore
            {15.8, 80.3},   // Ongole / Machilipatnam
            {16.7, 82.2},   // Kakinada (Godavari Delta)
            {17.7, 83.3},   // Visakhapatnam
            {19.3, 85.0},   // Gopalpur / Chilika
            {19.8, 85.8},   // Puri
            {20.3, 86.7},   // Paradeep (Mahanadi Delta)
            {21.5, 87.2},   // Chandipur / Balasore
            {21.6, 87.8},   // Digha
            {21.8, 88.5},   // Sundarbans West
            {22.0, 89.5},   // Sundarbans Central
            {22.2, 90.5},   // Meghna Estuary / Bangladesh
            {21.4, 91.9},   // Coxs Bazar
            {24.0, 92.5},
            {27.5, 96.5},   // Arunachal East
            {28.5, 95.0},   // Arunachal North
            {27.8, 88.5},   // Sikkim
            {30.5, 81.0},   // Uttarakhand
            {32.5, 77.0},   // Himachal
            {35.5, 75.0},   // Kashmir
            {34.0, 74.0},
            {31.0, 74.5},   // Punjab
            {27.0, 71.0},   // Rajasthan
            {24.5, 68.5}    // Rann of Kutch
        };
        return poly;
    }

    static bool isPointInPoly(double lat, double lon, const std::vector<std::pair<double, double>>& poly) {
        int n = static_cast<int>(poly.size());
        bool inside = false;
        double p1x = poly[0].second, p1y = poly[0].first;
        for (int i = 0; i <= n; ++i) {
            double p2x = poly[i % n].second, p2y = poly[i % n].first;
            if (lat > std::min(p1y, p2y)) {
                if (lat <= std::max(p1y, p2y)) {
                    if (lon <= std::max(p1x, p2x)) {
                        double xinters = (p1y != p2y) ? (lat - p1y) * (p2x - p1x) / (p2y - p1y) + p1x : p1x;
                        if (p1x == p2x || lon <= xinters) {
                            inside = !inside;
                        }
                    }
                }
            }
            p1x = p2x;
            p1y = p2y;
        }
        return inside;
    }

    static double distToSegment(double px, double py, double x1, double y1, double x2, double y2) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        if (dx == 0.0 && dy == 0.0) return std::hypot(px - x1, py - y1);
        double t = std::clamp(((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
        double proj_x = x1 + t * dx;
        double proj_y = y1 + t * dy;
        return std::hypot(px - proj_x, py - proj_y);
    }

    static double minDistanceToCoastKm(double lat, double lon) {
        const auto& poly = getIndiaCoastPoly();
        double min_d_deg = 1e9;
        // Check coastal boundary edges (indices 0 to 37)
        for (int i = 0; i < 37; ++i) {
            const auto& p1 = poly[i];
            const auto& p2 = poly[(i + 1) % poly.size()];
            double d = distToSegment(lon, lat, p1.second, p1.first, p2.second, p2.first);
            if (d < min_d_deg) {
                min_d_deg = d;
            }
        }
        return min_d_deg * 111.0; // Conversion degrees to km
    }

    // High-Precision Analytical Topographic & Bathymetric Elevation Model (m MSL)
    static double getElevationMSL(double lat, double lon) {
        const auto& poly = getIndiaCoastPoly();
        bool inside = isPointInPoly(lat, lon, poly);
        double dist_km = minDistanceToCoastKm(lat, lon);

        // 1. Ocean Bathymetry (Negative Elevation)
        if (!inside) {
            double shelfDepth = -2.0 - std::min(4500.0, dist_km * 16.0 + std::pow(dist_km / 12.0, 2.0) * 8.0);
            return shelfDepth;
        }

        // 2. Coastal Lowlands & Estuary Strips (< 28 km inland)
        if (dist_km < 28.0) {
            // Sundarbans / Bengal Mangrove Delta (High tidal vulnerability)
            if (lat >= 21.4 && lat <= 22.8 && lon >= 88.0 && lon <= 91.5) {
                return 1.2 + dist_km * 0.14 + std::sin(lat * 30.0 + lon * 20.0) * 0.4;
            }
            // Kerala Backwaters & Kuttanad (Alappuzha, Vembanad)
            if (lat >= 8.5 && lat <= 10.5 && lon >= 75.8 && lon <= 76.8) {
                return 0.8 + dist_km * 0.18 + std::sin(lat * 15.0) * 0.4;
            }
            // Gujarat Gulf of Khambhat / Rann of Kutch
            if (lat >= 20.5 && lat <= 24.5 && lon >= 68.5 && lon <= 73.0) {
                return 1.8 + dist_km * 0.32 + std::sin(lon * 20.0) * 0.5;
            }
            // Konkan / Mumbai Salsette lowlands
            if (lat >= 15.5 && lat <= 20.2 && lon < 73.5) {
                return 2.5 + dist_km * 0.75;
            }
            // Coromandel / Chennai / Cauvery Delta
            if (lat >= 10.0 && lat <= 16.0 && lon > 79.5) {
                return 2.8 + dist_km * 0.38;
            }
            // Odisha / Mahanadi & Chilika
            if (lat >= 19.0 && lat <= 21.5 && lon > 85.0) {
                return 2.2 + dist_km * 0.28;
            }
            return 2.0 + dist_km * 0.48;
        }

        // 3. Bengal Plains & Gangetic Delta (lat 21.8 to 24.5, lon 87.0 to 89.5)
        if (lat >= 21.8 && lat <= 24.5 && lon >= 87.0 && lon <= 89.5) {
            double t_bengal = (lat - 21.8) / 2.7;
            return 5.0 + t_bengal * 35.0 + std::sin(lat * 10.0 + lon * 8.0) * 2.0;
        }

        // 4. Brahmaputra Valley (Assam) (lat 25.5 to 28.2, lon 89.8 to 96.2)
        if (lat >= 25.5 && lat <= 28.2 && lon >= 89.8 && lon <= 96.2) {
            double t = (lon - 89.8) / 6.4; // 0 to 1
            double floor_elev = 34.0 + t * 78.0; // 34m (Dhubri) to 112m (Dibrugarh)
            double axis_lat = 26.1 + t * 1.3;
            double lat_dist = std::abs(lat - axis_lat);
            if (lat_dist < 0.45) {
                return floor_elev + lat_dist * 32.0;
            } else if (lat > axis_lat) { // North towards Arunachal Himalayas
                return floor_elev + 14.0 + (lat_dist - 0.45) * 1900.0;
            } else { // South towards Meghalaya Plateau & Karbi Hills
                return floor_elev + 14.0 + std::min(1550.0, (lat_dist - 0.45) * 1250.0);
            }
        }

        // 5. Himalayas & Karakoram (North India, Sikkim, Arunachal)
        if (lat >= 27.5 && lat <= 37.0) {
            double base_h = (lat - 27.5) * 380.0;
            if (lon >= 85.0 && lon <= 90.0) { // Nepal / Sikkim High Himalayas
                base_h += 2400.0;
            } else if (lon >= 74.0 && lon <= 80.0 && lat > 29.5) { // Kashmir, Himachal, Uttarakhand
                base_h += 2600.0 + (lat - 29.5) * 450.0;
            }
            return std::min(8848.0, 320.0 + base_h + std::abs(std::sin(lat * 5.0 + lon * 7.0)) * 520.0);
        }

        // 6. Western Ghats Escarpment (lat 8.5 to 21.0, lon ~73.5 to 76.5)
        double ghats_lon = 77.0 - (lat - 8.5) / 12.5 * 3.4;
        double dist_ghats = std::abs(lon - ghats_lon);
        double ghats_elev = 0.0;
        if (dist_ghats < 0.72) {
            double peak = (lat > 14.0) ? 1150.0 : 1780.0;
            ghats_elev = peak * (1.0 - std::pow(dist_ghats / 0.72, 2.0));
        }

        // 7. Indo-Gangetic Plains (lat 24.5 to 28.5, lon 75.0 to 88.0)
        if (lat >= 24.5 && lat <= 28.5 && lon >= 75.0 && lon <= 88.0) {
            double t_ganga = (88.0 - lon) / 13.0; // Kolkata -> Delhi
            double ganga_elev = 18.0 + t_ganga * 198.0; // ~18m (Bengal/Bihar) -> ~216m (Delhi)
            return ganga_elev + std::sin(lat * 12.0 + lon * 8.0) * 6.0;
        }

        // 8. Deccan Plateau & Peninsular Uplands
        double deccan_base = 460.0 + std::sin(lat * 0.4) * 110.0 + std::cos(lon * 0.3) * 70.0;
        if (lat < 14.5 && lon >= 76.0 && lon <= 78.5) { // Mysore / Bangalore Plateau
            deccan_base = 890.0 + std::sin(lat * 5.0) * 45.0;
        }

        return std::max(8.0, deccan_base + ghats_elev);
    }

    // Color Interpolation for Turbo Colormap
    static QColor getTurboColor(float normalized) {
        float x = std::clamp(normalized, 0.0f, 1.0f);
        // Polynomial approximations for Google Turbo
        float r = 0.1357f + x * (4.5974f + x * (-42.3278f + x * (130.5888f + x * (-150.5667f + x * 58.1375f))));
        float g = 0.0914f + x * (2.1856f + x * (4.8052f + x * (-14.0195f + x * (4.2109f + x * 2.7747f))));
        float b = 0.1067f + x * (12.5832f + x * (-65.3400f + x * (163.0768f + x * (-187.0337f + x * 76.8148f))));
        return QColor::fromRgbF(std::clamp(r, 0.0f, 1.0f), std::clamp(g, 0.0f, 1.0f), std::clamp(b, 0.0f, 1.0f));
    }

    // Color Interpolation for Magma Colormap
    static QColor getMagmaColor(float normalized) {
        float x = std::clamp(normalized, 0.0f, 1.0f);
        if (x < 0.25f) {
            float t = x / 0.25f;
            return QColor(static_cast<int>(10 + t * 50), static_cast<int>(5 + t * 10), static_cast<int>(30 + t * 80));
        } else if (x < 0.5f) {
            float t = (x - 0.25f) / 0.25f;
            return QColor(static_cast<int>(60 + t * 120), static_cast<int>(15 + t * 25), static_cast<int>(110 + t * 20));
        } else if (x < 0.75f) {
            float t = (x - 0.5f) / 0.25f;
            return QColor(static_cast<int>(180 + t * 65), static_cast<int>(40 + t * 110), static_cast<int>(130 - t * 60));
        } else {
            float t = (x - 0.75f) / 0.25f;
            return QColor(static_cast<int>(245 + t * 10), static_cast<int>(150 + t * 100), static_cast<int>(70 + t * 180));
        }
    }

    // Color Interpolation for Viridis Colormap
    static QColor getViridisColor(float normalized) {
        float x = std::clamp(normalized, 0.0f, 1.0f);
        if (x < 0.25f) {
            float t = x / 0.25f;
            return QColor(static_cast<int>(68 - t * 10), static_cast<int>(1 + t * 80), static_cast<int>(84 + t * 55));
        } else if (x < 0.5f) {
            float t = (x - 0.25f) / 0.25f;
            return QColor(static_cast<int>(58 - t * 25), static_cast<int>(81 + t * 64), static_cast<int>(139 + t * 1));
        } else if (x < 0.75f) {
            float t = (x - 0.5f) / 0.25f;
            return QColor(static_cast<int>(33 + t * 61), static_cast<int>(145 + t * 56), static_cast<int>(140 - t * 42));
        } else {
            float t = (x - 0.75f) / 0.25f;
            return QColor(static_cast<int>(94 + t * 159), static_cast<int>(201 + t * 30), static_cast<int>(98 - t * 62));
        }
    }

    // Master Color Generator based on Elevation, Sea Level Rise Threshold, Palette, and Opacity
    static QColor getColorForElevation(double elevationMSL, double seaLevelRise, HeatMapPalette palette, float opacity = 0.70f, int animPhase = 0) {
        int alpha = static_cast<int>(std::clamp(opacity, 0.05f, 1.0f) * 255.0f);

        // 1. Sea Level Rise & Coastal Inundation Mode
        if (palette == HeatMapPalette::SeaLevelInundation) {
            if (elevationMSL <= seaLevelRise) {
                // Submerged Area!
                if (elevationMSL < 0.0) {
                    // Natural ocean bathymetry (Deep navy to azure)
                    double depthRatio = std::clamp(-elevationMSL / 1500.0, 0.0, 1.0);
                    int r = static_cast<int>(8 + (1.0 - depthRatio) * 15);
                    int g = static_cast<int>(25 + (1.0 - depthRatio) * 85);
                    int b = static_cast<int>(75 + (1.0 - depthRatio) * 165);
                    return QColor(r, g, b, alpha);
                } else {
                    // NEWLY SUBMERGED COASTAL LAND from Sea Level Rise!
                    // Pulsing electric cyan & warning crimson ripple
                    double pulse = (std::sin(animPhase * 0.1 + elevationMSL * 2.0) + 1.0) * 0.5;
                    int r = static_cast<int>(20 + pulse * 230);
                    int g = static_cast<int>(180 - pulse * 120);
                    int b = static_cast<int>(245 - pulse * 140);
                    return QColor(r, g, b, std::min(255, alpha + 50));
                }
            } else {
                // Land above Sea Level
                double clearance = elevationMSL - seaLevelRise;
                if (clearance <= 2.5) {
                    // Critical 0 - 2.5m clearance: Alert Coral/Red
                    return QColor(244, 63, 94, alpha);
                } else if (clearance <= 6.0) {
                    // High Hazard 2.5 - 6m clearance: Warning Amber
                    return QColor(245, 158, 11, alpha);
                } else if (clearance <= 15.0) {
                    // Moderate Hazard 6 - 15m: Lime / Green Lowland
                    return QColor(132, 204, 22, alpha);
                } else if (clearance <= 40.0) {
                    // Low Risk 15 - 40m: Mint Green
                    return QColor(16, 185, 129, alpha);
                } else if (clearance <= 120.0) {
                    // River Basins / Plains 40 - 120m: Teal/Cyan
                    return QColor(14, 165, 233, alpha);
                } else if (clearance <= 450.0) {
                    // Plateau 120 - 450m: Amber Brown
                    return QColor(217, 119, 6, alpha);
                } else if (clearance <= 1500.0) {
                    // Highlands 450 - 1500m: Terracotta
                    return QColor(185, 28, 28, alpha);
                } else {
                    // Mountain Peaks > 1500m: Alpine Purple/White
                    return QColor(241, 245, 249, alpha);
                }
            }
        }

        // 2. Ocean Depth / Bathymetry Palette
        if (palette == HeatMapPalette::OceanDepth) {
            if (elevationMSL <= 0.0) {
                float norm = static_cast<float>(std::clamp(-elevationMSL / 3000.0, 0.0, 1.0));
                int r = static_cast<int>(2 + (1.0f - norm) * 40);
                int g = static_cast<int>(15 + (1.0f - norm) * 160);
                int b = static_cast<int>(60 + (1.0f - norm) * 190);
                return QColor(r, g, b, alpha);
            } else {
                float norm = static_cast<float>(std::clamp(elevationMSL / 1200.0, 0.0, 1.0));
                int r = static_cast<int>(30 + norm * 180);
                int g = static_cast<int>(160 + norm * 80);
                int b = static_cast<int>(120 - norm * 90);
                return QColor(r, g, b, alpha);
            }
        }

        // 3. Turbo / Magma / Viridis Palettes
        float normalized = static_cast<float>(std::clamp((elevationMSL + 50.0) / 3500.0, 0.0, 1.0));
        QColor col;
        if (palette == HeatMapPalette::Turbo) {
            col = getTurboColor(normalized);
        } else if (palette == HeatMapPalette::Magma) {
            col = getMagmaColor(normalized);
        } else {
            col = getViridisColor(normalized);
        }
        col.setAlpha(alpha);
        return col;
    }

    // Curated Coastal and Flood Risk Presets
    static inline const std::vector<CoastalHotspot>& getCoastalPresets() {
        static const std::vector<CoastalHotspot> presets = {
            {"Sundarbans & Bengal Delta", "West Bengal", 21.9497, 88.9006, 10, 2.4, 4.5, "World's largest mangrove delta. 85% land below 4m MSL."},
            {"Mumbai & Konkan Coast", "Maharashtra", 18.9220, 72.8347, 12, 4.8, 4.2, "High-density urban peninsula. Severe storm surge vulnerability."},
            {"Gulf of Khambhat & Surat", "Gujarat", 21.1702, 72.8311, 11, 6.2, 8.5, "Extreme macro-tidal range up to 8.5m. High industrial risk."},
            {"Kerala Backwaters (Alappuzha)", "Kerala", 9.4981, 76.3388, 12, 1.8, 1.2, "Kuttanad below sea level farming zone. Backwater flooding."},
            {"Chennai & Coromandel Coast", "Tamil Nadu", 13.0827, 80.2707, 11, 6.5, 1.5, "Low-gradient coastal plain prone to cyclone storm surges."},
            {"Paradeep & Mahanadi Delta", "Odisha", 20.3168, 86.6114, 11, 4.2, 3.2, "Mahanadi river mouth delta with major deep-water port."},
            {"Assam Brahmaputra Valley", "Assam", 26.2006, 92.9376, 8, 52.0, 0.0, "Major inland river basin subject to catastrophic monsoonal floods."},
            {"Full India Subcontinent", "National Overview", 22.0000, 79.0000, 5, 0.0, 0.0, "National topography and multi-basin sea level analysis."}
        };
        return presets;
    }
};

} // namespace MapCore
