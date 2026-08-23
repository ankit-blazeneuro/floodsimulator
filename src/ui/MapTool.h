#pragma once

#include <cstdint>

namespace MapUI {

enum class MapTool : uint8_t {
    Move = 0,    // Default Pan / Navigation
    Select,      // Box Selection & Inspect
    Rotate,      // 360° Map Rotation around Screen Middle Crosshair
    Ruler        // Distance Measurement
};

} // namespace MapUI
