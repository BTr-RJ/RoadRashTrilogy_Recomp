#include "presentation/interpolation.hpp"
#include "presentation/geometry.h"
#include <cmath>
#include <iostream>
#include <limits>

namespace p = rr::presentation;
static int checks = 0;
static int failures = 0;
static void check(bool condition, const char* message) {
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
static bool close(double a, double b) { return std::abs(a - b) < 1e-9; }

int main() {
    check(close(rr_display_aspect(320,320),4.0/3.0), "H40 targets 4:3");
    check(close(rr_display_aspect(256,256),4.0/3.0), "H32 targets 4:3 without squeezed artwork");
    check(close(rr_display_aspect(428,320)/(4.0/3.0),428.0/320.0), "expanded H40 retains native pixel scale");
    check(close(rr_display_aspect(342,256)/(4.0/3.0),342.0/256.0), "expanded H32 retains native pixel scale");
    const p::Camera camera{{0, 2, 0}, 100, 640, 240, 0.1};
    const p::Actor previous{1, 0, {0, 0, 20}, 2, 4};
    const p::Actor current{1, 0, {0, 0, 10}, 2, 4};
    auto halfway = p::project(previous, current, 0.5, true, camera);
    check(halfway.has_value() && close(halfway->depth, 15), "interpolate world depth");
    check(halfway.has_value() && close(halfway->width, 200.0 / 15), "perspective zoom from sampled depth");
    check(halfway.has_value() && close(halfway->x + halfway->width * 0.5, 640), "center anchor stable");
    check(halfway.has_value() && close(halfway->y + halfway->height, 240 + 200.0 / 15), "feet anchor on projected ground");
    double last_width = 0;
    bool monotonic = true;
    for (int frame = 0; frame <= 60; ++frame) {
        const auto sprite = p::project(previous, current, frame / 60.0, true, camera);
        if (!sprite || sprite->width < last_width) monotonic = false;
        if (sprite) last_width = sprite->width;
    }
    check(monotonic, "approaching actor grows continuously");
    auto reused = current;
    reused.generation = 1;
    check(close(p::sample_feet(previous, reused, 0.5, true).z, 10), "slot reuse resets history");
    check(close(p::sample_feet(previous, current, 0.5, false).z, 10), "rollback or camera cut resets history");
    check(close(p::sample_feet(previous, current, -1, true).z, 20), "negative alpha clamped");
    check(close(p::sample_feet(previous, current, 2, true).z, 10), "large alpha clamped");
    check(close(p::sample_feet(previous, current, std::numeric_limits<double>::quiet_NaN(), true).z, 10), "invalid alpha uses current");
    auto behind = current;
    behind.feet.z = 0;
    check(!p::project(behind, behind, 1, true, camera), "near clipping avoids division by zero");
    auto bad_camera = camera;
    bad_camera.focal_pixels = std::numeric_limits<double>::infinity();
    check(!p::project(previous, current, 1, true, bad_camera), "invalid projection rejected");
    // Rendering samples must not modify authoritative actor state.
    check(close(previous.feet.z, 20) && close(current.feet.z, 10), "simulation inputs unchanged");
    std::cout << checks << " presentation checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
