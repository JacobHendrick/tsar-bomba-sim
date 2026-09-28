#pragma once
#include <algorithm>
#include <cmath>
#include <vector>
namespace phys {
    constexpr double kJPerMt = 4.184e15, kG = 9.80665, kR = 287.053, kKappa = 0.2857, kP0 = 101325.0;
    struct Air { double T, p, rho, theta; };
    inline Air ussa76(double z) {
        static const double hb[] = {0, 11e3, 20e3, 32e3, 47e3, 51e3, 71e3, 84.852e3};
        static const double lb[] = {-6.5e-3, 0, 1e-3, 2.8e-3, 0, -2.8e-3, -2e-3};
        double h = std::clamp(6356766.0 * z / (6356766.0 + z), 0.0, hb[7]), T = 288.15, p = kP0;
        for (int i = 0; i < 7 && h > hb[i]; ++i) {
            double dh = std::min(h, hb[i + 1]) - hb[i], T1 = T + lb[i] * dh;
            p *= lb[i] == 0 ? std::exp(-kG * dh / (kR * T)) : std::pow(T / T1, kG / (kR * lb[i]));
            T = T1;
        }
        return {T, p, p / (kR * T), T * std::pow(kP0 / p, kKappa)};
    }
    inline std::vector<float> atmTable(double height, int n) {
        std::vector<float> v;
        for (int i = 0; i < n; ++i) {
            Air a = ussa76((i + 0.5) / n * height);
            v.insert(v.end(), {float(a.T), float(a.rho), float(a.theta), float(a.p)});
        }
        return v;
    }
    struct Preset { const char* name; double yieldMt, hob; };
    inline const Preset kPresets[] = {{"Tsar Bomba 50 Mt", 50.0, 4000.0}, {"1 Mt airburst", 1.0, 2000.0}};
    struct Burst {
        const char* name;
        double W, E, hob, rho, cs, Rmax, base, t2, tSeed, radK, heat, scale, width, height;
        explicit Burst(const Preset& p) : name(p.name), W(p.yieldMt), E(p.yieldMt * kJPerMt), hob(p.hob) {
            Air a = ussa76(hob);
            rho = a.rho;
            cs = std::sqrt(1.4 * kR * a.T);
            Rmax = 1000.0 * std::pow(W, 0.4);
            base = std::max(hob - Rmax, 0.3 * hob);
            t2 = std::sqrt(W);
            tSeed = 2.0 * t2;
            radK = 4e-11 / std::sqrt(W);
            heat = 5.0;
            scale = std::pow(W / 50.0, 0.25);
            width = 160e3 * scale;
            height = 80e3 * scale;
        }
        double sedov(double t) const { return 1.03 * std::pow(E * t * t / rho, 0.2); }
        double shockR(double t) const {
            double tc = std::pow(0.4 * 1.03 * std::pow(E / rho, 0.2) / cs, 1.0 / 0.6);
            return t < tc ? sedov(t) : sedov(tc) + cs * (t - tc);
        }
        double groundShock(double t) const { double r = shockR(t); return std::sqrt(std::max(r * r - hob * hob, 0.0)); }
        double fireballR(double t) const { return std::min(shockR(t), Rmax); }
        double temperature(double t) const {
            static const double k[][2] = {{1e-5, 2500}, {7e-4, 20000}, {0.01, 9000}, {0.078, 2800}, {0.3, 5500},
                                          {1, 7500},    {3, 4200},      {10, 1800},   {30, 900},    {100, 350}};
            double x = std::log(std::max(t / t2, 1e-5));
            for (int i = 0; i < 9; ++i) {
                double a = std::log(k[i][0]), b = std::log(k[i + 1][0]);
                if (x > b) continue;
                double f = std::clamp((x-a) / (b - a), 0.0, 1.0);
                f = f * f *(3 - 2 * f);
                return std::exp(std::log(k[i][1]) + f * (std::log(k[i + 1][1]) - std::log(k[i][1])));
            }
            return 350.0;
        }
    };
}
