#pragma once
#include <cstdint>

class Time {
public:
    static void   Init();
    static void   Tick();
    static double DeltaTime();   // dt del último frame (seg)
    static double ElapsedTime(); // tiempo total (seg)
    static double FPS();

private:
    inline static double s_delta = 0.0;
    inline static double s_time  = 0.0;
    inline static double s_fps   = 0.0;
    inline static uint64_t s_prevTicks = 0;
    inline static double s_freqInv = 0.0;
};
