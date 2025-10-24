#include "core/Application.h"
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#endif

int main(int, char**) {
    try {
        Application app;
        app.Run();
    }
    catch (const std::exception& ex) {
#ifdef _WIN32
        MessageBoxA(nullptr, ex.what(), "SandboxCity - Fatal error", MB_ICONERROR | MB_OK);
#endif
        std::cerr << "[FATAL] " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
