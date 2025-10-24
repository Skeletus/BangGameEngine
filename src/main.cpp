#include <cstdio>
#include <exception>
#include <cstdlib>
#ifdef _WIN32
  #include <windows.h>
#endif

// Tu App
#include "core/Application.h"

int main()
{
#ifdef _WIN32
    // Para que la consola muestre UTF-8 (acentos) correctamente.
    SetConsoleOutputCP(CP_UTF8);
#endif

    try {
        Application app;
        app.Run();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "[FATAL] Excepción: %s\n", e.what());
#ifdef _WIN32
        Sleep(4000); // deja 4s para leer el mensaje en la consola
#endif
        return EXIT_FAILURE;
    }
    catch (...) {
        std::fprintf(stderr, "[FATAL] Excepción desconocida.\n");
#ifdef _WIN32
        Sleep(4000);
#endif
        return EXIT_FAILURE;
    }
}
