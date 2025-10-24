#include "Time.h"
#include <chrono>
#include <cstdint>

void Time::Init() {
    using clock = std::chrono::steady_clock; // estable para medir intervalos
    const auto now = clock::now().time_since_epoch();
    s_prevTicks = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

    s_freqInv = 1.0 / 1e9; // ns -> s
    s_delta = 0.0;
    s_time = 0.0;
    s_fps = 0.0;
}

void Time::Tick() {
    using clock = std::chrono::steady_clock;
    const uint64_t nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        clock::now().time_since_epoch()).count();

    const uint64_t diff = nowNs - s_prevTicks;
    s_prevTicks = nowNs;

    s_delta = static_cast<double>(diff) * s_freqInv;
    if (s_delta < 0.0) s_delta = 0.0; // por seguridad si el reloj retrocede
    s_time += s_delta;

    s_fps = (s_delta > 0.0) ? (1.0 / s_delta) : 0.0;
}

// === getters requeridos por el linker ===
double Time::DeltaTime() { return s_delta; }
double Time::ElapsedTime() { return s_time; }
double Time::FPS() { return s_fps; }
