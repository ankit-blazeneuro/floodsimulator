#include "MapDataCache.h"
#include <fstream>
#include <iostream>
#include <chrono>
#include <sys/stat.h>

namespace MapCore {

// Helper for writing strings
static void writeString(std::ostream& os, const std::string& str) {
    uint16_t len = static_cast<uint16_t>(str.size());
    os.write(reinterpret_cast<const char*>(&len), sizeof(len));
    if (len > 0) {
        os.write(str.data(), len);
    }
}

static std::string readString(std::istream& is) {
    uint16_t len = 0;
    is.read(reinterpret_cast<char*>(&len), sizeof(len));
    if (len == 0) return "";
    std::string str(len, '\0');
    is.read(&str[0], len);
    return str;
}

bool MapDataCache::isCacheValid(const std::string& cachePath, const std::string& sourcePbfPath) {
    struct stat cacheStat, pbfStat;
    if (stat(cachePath.c_str(), &cacheStat) != 0) return false;
    if (stat(sourcePbfPath.c_str(), &pbfStat) != 0) return false;

    // Cache must be at least as recent as PBF
    if (cacheStat.st_mtime < pbfStat.st_mtime) return false;
    if (cacheStat.st_size < 1024) return false;

    // Check header magic
    std::ifstream is(cachePath, std::ios::binary);
    if (!is.is_open()) return false;

    uint32_t magic = 0, ver = 0;
    is.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    is.read(reinterpret_cast<char*>(&ver), sizeof(ver));

    return (magic == CACHE_MAGIC && ver == CACHE_VERSION);
}

bool MapDataCache::saveCache(const std::string& cachePath, const SpatialIndex& index) {
    auto t0 = std::chrono::high_resolution_clock::now();

    std::ofstream os(cachePath, std::ios::binary | std::ios::trunc);
    if (!os.is_open()) {
        std::cerr << "MapDataCache: Failed to create cache file " << cachePath << std::endl;
        return false;
    }

    uint32_t magic = CACHE_MAGIC;
    uint32_t ver = CACHE_VERSION;
    os.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    os.write(reinterpret_cast<const char*>(&ver), sizeof(ver));

    // Extent
    os.write(reinterpret_cast<const char*>(&index.extent.minX), sizeof(float));
    os.write(reinterpret_cast<const char*>(&index.extent.minY), sizeof(float));
    os.write(reinterpret_cast<const char*>(&index.extent.maxX), sizeof(float));
    os.write(reinterpret_cast<const char*>(&index.extent.maxY), sizeof(float));

    // Polygons
    uint32_t polyCount = static_cast<uint32_t>(index.polygons.size());
    os.write(reinterpret_cast<const char*>(&polyCount), sizeof(polyCount));
    for (const auto& poly : index.polygons) {
        os.write(reinterpret_cast<const char*>(&poly.id), sizeof(poly.id));
        uint8_t cat = static_cast<uint8_t>(poly.category);
        os.write(reinterpret_cast<const char*>(&cat), sizeof(cat));
        os.write(reinterpret_cast<const char*>(&poly.minZoom), sizeof(poly.minZoom));
        os.write(reinterpret_cast<const char*>(&poly.maxZoom), sizeof(poly.maxZoom));
        writeString(os, poly.name);

        os.write(reinterpret_cast<const char*>(&poly.bbox.minX), sizeof(float));
        os.write(reinterpret_cast<const char*>(&poly.bbox.minY), sizeof(float));
        os.write(reinterpret_cast<const char*>(&poly.bbox.maxX), sizeof(float));
        os.write(reinterpret_cast<const char*>(&poly.bbox.maxY), sizeof(float));

        uint32_t ptCount = static_cast<uint32_t>(poly.points.size());
        os.write(reinterpret_cast<const char*>(&ptCount), sizeof(ptCount));
        if (ptCount > 0) {
            os.write(reinterpret_cast<const char*>(poly.points.data()), ptCount * sizeof(Point2D));
        }

        uint16_t tagCount = static_cast<uint16_t>(poly.tags.size());
        os.write(reinterpret_cast<const char*>(&tagCount), sizeof(tagCount));
        for (const auto& kv : poly.tags) {
            writeString(os, kv.first);
            writeString(os, kv.second);
        }
    }

    // Polylines
    uint32_t lineCount = static_cast<uint32_t>(index.polylines.size());
    os.write(reinterpret_cast<const char*>(&lineCount), sizeof(lineCount));
    for (const auto& line : index.polylines) {
        os.write(reinterpret_cast<const char*>(&line.id), sizeof(line.id));
        uint8_t cat = static_cast<uint8_t>(line.category);
        os.write(reinterpret_cast<const char*>(&cat), sizeof(cat));
        os.write(reinterpret_cast<const char*>(&line.minZoom), sizeof(line.minZoom));
        os.write(reinterpret_cast<const char*>(&line.maxZoom), sizeof(line.maxZoom));
        writeString(os, line.name);
        writeString(os, line.ref);

        os.write(reinterpret_cast<const char*>(&line.bbox.minX), sizeof(float));
        os.write(reinterpret_cast<const char*>(&line.bbox.minY), sizeof(float));
        os.write(reinterpret_cast<const char*>(&line.bbox.maxX), sizeof(float));
        os.write(reinterpret_cast<const char*>(&line.bbox.maxY), sizeof(float));

        uint32_t ptCount = static_cast<uint32_t>(line.points.size());
        os.write(reinterpret_cast<const char*>(&ptCount), sizeof(ptCount));
        if (ptCount > 0) {
            os.write(reinterpret_cast<const char*>(line.points.data()), ptCount * sizeof(Point2D));
        }

        uint32_t lodCount = static_cast<uint32_t>(line.lodPoints.size());
        os.write(reinterpret_cast<const char*>(&lodCount), sizeof(lodCount));
        if (lodCount > 0) {
            os.write(reinterpret_cast<const char*>(line.lodPoints.data()), lodCount * sizeof(Point2D));
        }

        uint16_t tagCount = static_cast<uint16_t>(line.tags.size());
        os.write(reinterpret_cast<const char*>(&tagCount), sizeof(tagCount));
        for (const auto& kv : line.tags) {
            writeString(os, kv.first);
            writeString(os, kv.second);
        }
    }

    // Points
    uint32_t pointCount = static_cast<uint32_t>(index.points.size());
    os.write(reinterpret_cast<const char*>(&pointCount), sizeof(pointCount));
    for (const auto& pt : index.points) {
        os.write(reinterpret_cast<const char*>(&pt.id), sizeof(pt.id));
        uint8_t cat = static_cast<uint8_t>(pt.category);
        os.write(reinterpret_cast<const char*>(&cat), sizeof(cat));
        os.write(reinterpret_cast<const char*>(&pt.pos.x), sizeof(float));
        os.write(reinterpret_cast<const char*>(&pt.pos.y), sizeof(float));
        os.write(reinterpret_cast<const char*>(&pt.minZoom), sizeof(pt.minZoom));
        os.write(reinterpret_cast<const char*>(&pt.maxZoom), sizeof(pt.maxZoom));
        os.write(reinterpret_cast<const char*>(&pt.priority), sizeof(pt.priority));
        writeString(os, pt.name);
        writeString(os, pt.categoryLabel);

        uint16_t tagCount = static_cast<uint16_t>(pt.tags.size());
        os.write(reinterpret_cast<const char*>(&tagCount), sizeof(tagCount));
        for (const auto& kv : pt.tags) {
            writeString(os, kv.first);
            writeString(os, kv.second);
        }
    }

    // Search Items
    uint32_t sCount = static_cast<uint32_t>(index.searchItems.size());
    os.write(reinterpret_cast<const char*>(&sCount), sizeof(sCount));
    for (const auto& s : index.searchItems) {
        writeString(os, s.name);
        writeString(os, s.detail);
        uint8_t cat = static_cast<uint8_t>(s.category);
        os.write(reinterpret_cast<const char*>(&cat), sizeof(cat));
        os.write(reinterpret_cast<const char*>(&s.pos.x), sizeof(float));
        os.write(reinterpret_cast<const char*>(&s.pos.y), sizeof(float));
        os.write(reinterpret_cast<const char*>(&s.bounds.minX), sizeof(float));
        os.write(reinterpret_cast<const char*>(&s.bounds.minY), sizeof(float));
        os.write(reinterpret_cast<const char*>(&s.bounds.maxX), sizeof(float));
        os.write(reinterpret_cast<const char*>(&s.bounds.maxY), sizeof(float));
        os.write(reinterpret_cast<const char*>(&s.zoomTarget), sizeof(s.zoomTarget));
        os.write(reinterpret_cast<const char*>(&s.priority), sizeof(s.priority));
    }

    auto t1 = std::chrono::high_resolution_clock::now();
    double dur = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "MapDataCache: Saved cache to " << cachePath << " in " << dur << "s" << std::endl;
    return true;
}

bool MapDataCache::loadCache(const std::string& cachePath, SpatialIndex& index) {
    auto t0 = std::chrono::high_resolution_clock::now();

    std::ifstream is(cachePath, std::ios::binary);
    if (!is.is_open()) return false;

    uint32_t magic = 0, ver = 0;
    is.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    is.read(reinterpret_cast<char*>(&ver), sizeof(ver));

    if (magic != CACHE_MAGIC || ver != CACHE_VERSION) {
        std::cerr << "MapDataCache: Version mismatch or corrupted cache file." << std::endl;
        return false;
    }

    index.clear();

    // Extent
    is.read(reinterpret_cast<char*>(&index.extent.minX), sizeof(float));
    is.read(reinterpret_cast<char*>(&index.extent.minY), sizeof(float));
    is.read(reinterpret_cast<char*>(&index.extent.maxX), sizeof(float));
    is.read(reinterpret_cast<char*>(&index.extent.maxY), sizeof(float));

    // Polygons
    uint32_t polyCount = 0;
    is.read(reinterpret_cast<char*>(&polyCount), sizeof(polyCount));
    index.polygons.resize(polyCount);
    for (uint32_t i = 0; i < polyCount; ++i) {
        auto& poly = index.polygons[i];
        is.read(reinterpret_cast<char*>(&poly.id), sizeof(poly.id));
        uint8_t cat = 0;
        is.read(reinterpret_cast<char*>(&cat), sizeof(cat));
        poly.category = static_cast<FeatureCategory>(cat);
        is.read(reinterpret_cast<char*>(&poly.minZoom), sizeof(poly.minZoom));
        is.read(reinterpret_cast<char*>(&poly.maxZoom), sizeof(poly.maxZoom));
        poly.name = readString(is);

        is.read(reinterpret_cast<char*>(&poly.bbox.minX), sizeof(float));
        is.read(reinterpret_cast<char*>(&poly.bbox.minY), sizeof(float));
        is.read(reinterpret_cast<char*>(&poly.bbox.maxX), sizeof(float));
        is.read(reinterpret_cast<char*>(&poly.bbox.maxY), sizeof(float));

        uint32_t ptCount = 0;
        is.read(reinterpret_cast<char*>(&ptCount), sizeof(ptCount));
        poly.points.resize(ptCount);
        if (ptCount > 0) {
            is.read(reinterpret_cast<char*>(poly.points.data()), ptCount * sizeof(Point2D));
        }

        uint16_t tagCount = 0;
        is.read(reinterpret_cast<char*>(&tagCount), sizeof(tagCount));
        poly.tags.resize(tagCount);
        for (uint16_t t = 0; t < tagCount; ++t) {
            poly.tags[t].first = readString(is);
            poly.tags[t].second = readString(is);
        }
    }

    // Polylines
    uint32_t lineCount = 0;
    is.read(reinterpret_cast<char*>(&lineCount), sizeof(lineCount));
    index.polylines.resize(lineCount);
    for (uint32_t i = 0; i < lineCount; ++i) {
        auto& line = index.polylines[i];
        is.read(reinterpret_cast<char*>(&line.id), sizeof(line.id));
        uint8_t cat = 0;
        is.read(reinterpret_cast<char*>(&cat), sizeof(cat));
        line.category = static_cast<FeatureCategory>(cat);
        is.read(reinterpret_cast<char*>(&line.minZoom), sizeof(line.minZoom));
        is.read(reinterpret_cast<char*>(&line.maxZoom), sizeof(line.maxZoom));
        line.name = readString(is);
        line.ref = readString(is);

        is.read(reinterpret_cast<char*>(&line.bbox.minX), sizeof(float));
        is.read(reinterpret_cast<char*>(&line.bbox.minY), sizeof(float));
        is.read(reinterpret_cast<char*>(&line.bbox.maxX), sizeof(float));
        is.read(reinterpret_cast<char*>(&line.bbox.maxY), sizeof(float));

        uint32_t ptCount = 0;
        is.read(reinterpret_cast<char*>(&ptCount), sizeof(ptCount));
        line.points.resize(ptCount);
        if (ptCount > 0) {
            is.read(reinterpret_cast<char*>(line.points.data()), ptCount * sizeof(Point2D));
        }

        uint32_t lodCount = 0;
        is.read(reinterpret_cast<char*>(&lodCount), sizeof(lodCount));
        line.lodPoints.resize(lodCount);
        if (lodCount > 0) {
            is.read(reinterpret_cast<char*>(line.lodPoints.data()), lodCount * sizeof(Point2D));
        }

        uint16_t tagCount = 0;
        is.read(reinterpret_cast<char*>(&tagCount), sizeof(tagCount));
        line.tags.resize(tagCount);
        for (uint16_t t = 0; t < tagCount; ++t) {
            line.tags[t].first = readString(is);
            line.tags[t].second = readString(is);
        }
    }

    // Points
    uint32_t pointCount = 0;
    is.read(reinterpret_cast<char*>(&pointCount), sizeof(pointCount));
    index.points.resize(pointCount);
    for (uint32_t i = 0; i < pointCount; ++i) {
        auto& pt = index.points[i];
        is.read(reinterpret_cast<char*>(&pt.id), sizeof(pt.id));
        uint8_t cat = 0;
        is.read(reinterpret_cast<char*>(&cat), sizeof(cat));
        pt.category = static_cast<FeatureCategory>(cat);
        is.read(reinterpret_cast<char*>(&pt.pos.x), sizeof(float));
        is.read(reinterpret_cast<char*>(&pt.pos.y), sizeof(float));
        is.read(reinterpret_cast<char*>(&pt.minZoom), sizeof(pt.minZoom));
        is.read(reinterpret_cast<char*>(&pt.maxZoom), sizeof(pt.maxZoom));
        is.read(reinterpret_cast<char*>(&pt.priority), sizeof(pt.priority));
        pt.name = readString(is);
        pt.categoryLabel = readString(is);

        uint16_t tagCount = 0;
        is.read(reinterpret_cast<char*>(&tagCount), sizeof(tagCount));
        pt.tags.resize(tagCount);
        for (uint16_t t = 0; t < tagCount; ++t) {
            pt.tags[t].first = readString(is);
            pt.tags[t].second = readString(is);
        }
    }

    // Search Items
    uint32_t sCount = 0;
    is.read(reinterpret_cast<char*>(&sCount), sizeof(sCount));
    index.searchItems.resize(sCount);
    for (uint32_t i = 0; i < sCount; ++i) {
        auto& s = index.searchItems[i];
        s.name = readString(is);
        s.detail = readString(is);
        uint8_t cat = 0;
        is.read(reinterpret_cast<char*>(&cat), sizeof(cat));
        s.category = static_cast<FeatureCategory>(cat);
        is.read(reinterpret_cast<char*>(&s.pos.x), sizeof(float));
        is.read(reinterpret_cast<char*>(&s.pos.y), sizeof(float));
        is.read(reinterpret_cast<char*>(&s.bounds.minX), sizeof(float));
        is.read(reinterpret_cast<char*>(&s.bounds.minY), sizeof(float));
        is.read(reinterpret_cast<char*>(&s.bounds.maxX), sizeof(float));
        is.read(reinterpret_cast<char*>(&s.bounds.maxY), sizeof(float));
        is.read(reinterpret_cast<char*>(&s.zoomTarget), sizeof(s.zoomTarget));
        is.read(reinterpret_cast<char*>(&s.priority), sizeof(s.priority));
    }

    // Rebuild spatial grid
    index.buildIndex();

    auto t1 = std::chrono::high_resolution_clock::now();
    double dur = std::chrono::duration<double>(t1 - t0).count();
    std::cout << "MapDataCache: Loaded binary cache in " << dur << "s | Features: "
              << (polyCount + lineCount + pointCount) << std::endl;

    return true;
}

} // namespace MapCore
