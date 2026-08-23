#pragma once

#include "SpatialIndex.h"
#include <string>
#include <functional>

namespace MapCore {

class OsmPbfLoader {
public:
    using ProgressCallback = std::function<void(float progress, const std::string& message)>;

    static bool loadPbfFile(const std::string& filepath, SpatialIndex& index,
                            ProgressCallback callback = nullptr);
};

} // namespace MapCore
