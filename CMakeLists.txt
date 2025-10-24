cmake_minimum_required(VERSION 3.20)

project(SandboxCity LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paquetes vcpkg
find_package(bgfx CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

# === Fuentes ===
file(GLOB_RECURSE SANDBOXCITY_SOURCES
  CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_executable(SandboxCity ${SANDBOXCITY_SOURCES})

target_include_directories(SandboxCity PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(SandboxCity PRIVATE 
  GLFW_EXPOSE_NATIVE_WIN32
)

# *** IMPORTANTE para bx con MSVC ***
# *** IMPORTANTE para bx con MSVC ***
if (MSVC)
  target_compile_options(SandboxCity PRIVATE
    /Zc:__cplusplus
    /Zc:preprocessor       # ← añade esto
    /permissive-
    /EHsc
  )
endif()


# Vincular librerías
target_link_libraries(SandboxCity PRIVATE
  bgfx::bgfx
  glfw
)

# Salida por configuración (VS multi-config)
set_target_properties(SandboxCity PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
