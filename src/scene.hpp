#pragma once
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

// Visual staging only: the 24-second introduction is deliberately time-compressed.
namespace scene {
constexpr double releaseTime = 5.0, introDuration = 24.0;
constexpr float releaseHeight = 10500.f;
constexpr float tsarFireballRadius = 4000.f;
constexpr float tsarCloudHeight = 64000.f, tsarCloudWidth = 95000.f;
inline float ease(double a, double b, double t) {
    float x = float(std::clamp((t - a) / (b - a), 0.0, 1.0));
    return x * x * (3.f - 2.f * x);
}
struct Flight {
    glm::vec3 aircraft{0}, bomb{0};
    float tilt = 0, canopy = 0, propellerAngle = 0;
    bool bombVisible = true;
};
inline Flight flight(double seconds, float burstHeight, bool detonated = false) {
    Flight f;
    double fall = std::clamp((seconds - releaseTime) / (introDuration - releaseTime), 0.0, 1.0);
    double travel = seconds < releaseTime ? (seconds - releaseTime) * 210.0 : fall * 42000.0;
    f.aircraft = {float(-2000.0 + travel), releaseHeight, 0};
    f.bomb = seconds < releaseTime ? f.aircraft + glm::vec3(0, -3.5f, 0)
        : glm::vec3(-2000.f * float(std::pow(1.0 - fall, 3.0)),
                    glm::mix(releaseHeight - 3.5f, burstHeight, float(std::pow(fall, 1.2))), 0);
    f.tilt = ease(releaseTime, releaseTime + 1.8, seconds);
    f.canopy = ease(releaseTime + 0.8, releaseTime + 2.8, seconds);
    f.propellerAngle = float(std::fmod(seconds * 45.0, 6.283185307));
    f.bombVisible = !detonated;
    if (detonated) f.aircraft.x += float(std::max(0.0, seconds - introDuration) * 210.0);
    return f;
}
}
