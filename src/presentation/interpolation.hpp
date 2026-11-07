#pragma once
// Presentation-only math. Not yet connected to any Road Rash game adapter.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

namespace rr::presentation {
struct Vec3 { double x, y, z; };
struct Camera {
    Vec3 position;
    double focal_pixels;
    double center_x;
    double horizon_y;
    double near_z;
};
struct Actor {
    std::uint32_t slot;
    std::uint32_t generation; // Increment when a slot is reused or teleported.
    Vec3 feet;
    double world_width;
    double world_height;
};
struct SpriteRect { double x, y, width, height, depth; };

inline bool finite(Vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

inline double fraction(double alpha) {
    return std::isfinite(alpha) ? std::clamp(alpha, 0.0, 1.0) : 1.0;
}

inline Vec3 interpolate(Vec3 previous, Vec3 current, double alpha) {
    const auto t = fraction(alpha);
    return {previous.x + (current.x - previous.x) * t,
            previous.y + (current.y - previous.y) * t,
            previous.z + (current.z - previous.z) * t};
}

inline Vec3 sample_feet(const Actor& previous, const Actor& current,
                        double alpha, bool history_valid) {
    if (!history_valid || previous.slot != current.slot ||
        previous.generation != current.generation || !finite(previous.feet)) {
        return current.feet;
    }
    return interpolate(previous.feet, current.feet, alpha);
}

// Interpolate world position BEFORE perspective projection. The same sampled
// camera must be used by the road, roadside objects and riders in a frame.
inline std::optional<SpriteRect> project(const Actor& previous, const Actor& current,
                                         double alpha, bool history_valid,
                                         const Camera& camera) {
    const Vec3 feet = sample_feet(previous, current, alpha, history_valid);
    if (!finite(feet) || !finite(camera.position) ||
        !std::isfinite(camera.focal_pixels) || camera.focal_pixels <= 0.0 ||
        !std::isfinite(camera.near_z) || camera.near_z <= 0.0 ||
        !std::isfinite(camera.center_x) || !std::isfinite(camera.horizon_y) ||
        !std::isfinite(current.world_width) || current.world_width <= 0.0 ||
        !std::isfinite(current.world_height) || current.world_height <= 0.0) {
        return std::nullopt;
    }
    const auto depth = feet.z - camera.position.z;
    if (depth < camera.near_z) return std::nullopt;
    const auto scale = camera.focal_pixels / depth;
    const auto width = current.world_width * scale;
    const auto height = current.world_height * scale;
    const auto feet_x = camera.center_x + (feet.x - camera.position.x) * scale;
    const auto feet_y = camera.horizon_y - (feet.y - camera.position.y) * scale;
    SpriteRect result{feet_x - width * 0.5, feet_y - height, width, height, depth};
    if (!std::isfinite(result.x) || !std::isfinite(result.y) ||
        !std::isfinite(width) || !std::isfinite(height) || !std::isfinite(depth)) {
        return std::nullopt;
    }
    return result;
}
} // namespace rr::presentation
