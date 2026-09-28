#include "scene.hpp"
#include <cstdio>
#include <stdexcept>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
int main() {
    try {
        auto attached = scene::flight(scene::releaseTime - 1e-5, 4000);
        auto released = scene::flight(scene::releaseTime, 4000);
        require(glm::length(attached.bomb - released.bomb) < 0.01f, "bomb jumps at release");
        require(released.canopy == 0 && released.tilt == 0, "canopy opens before release");
        auto open = scene::flight(10, 4000);
        require(open.canopy == 1 && open.tilt == 1 && open.bombVisible, "parachute stage is incomplete");
        for (float height : {2000.f, 4000.f}) {
            float previous = scene::releaseHeight;
            for (int i = 0; i <= 190; ++i) {
                auto f = scene::flight(scene::releaseTime + i * 0.1, height);
                require(std::isfinite(f.bomb.y) && f.bomb.y <= previous && f.bomb.y >= height,
                        "descent must be continuous, downward, and above burst height");
                previous = f.bomb.y;
            }
            auto end = scene::flight(scene::introDuration, height);
            require(glm::length(end.bomb - glm::vec3(0,height,0)) < 0.01f, "drop misses the visual detonation point");
            require(!scene::flight(scene::introDuration, height, true).bombVisible, "bomb remains after detonation");
        }
        require(scene::ease(30,480,0) == 0 && scene::ease(30,480,600) == 1, "camera easing exceeds its endpoints");
        std::puts("Scene timeline checks passed");
    } catch (const std::exception& e) { std::fprintf(stderr,"%s\n",e.what()); return 1; }
}
