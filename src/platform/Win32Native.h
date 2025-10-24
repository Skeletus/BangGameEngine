#pragma once
// Header intencionalmente mínimo: si más adelante necesitamos utilidades Win32,
// las agregamos aquí (por ejemplo, DPI awareness, paths a AppData, etc.)
#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif
