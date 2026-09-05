#pragma once

#include "SpatialIndex.h"
#include <string>

namespace MapCore {

class MapDataCache {
public:
    static constexpr uint32_t CACHE_MAGIC = 0x4153534D; // "ASSM"
    static constexpr uint32_t CACHE_VERSION = 2;

    static bool isCacheValid(const std::string& cachePath, const std::string& sourcePbfPath);
    static bool saveCache(const std::string& cachePath, const SpatialIndex& index);
    static bool loadCache(const std::string& cachePath, SpatialIndex& index);
};

} // namespace MapCore
