cmake_minimum_required(VERSION 3.20)

project(SandboxCity LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Paquetes vcpkg
find_package(bgfx CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)

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

option(SANDBOXCITY_KEEP_LEGACY_DRAWS "Mantener los draws directos previos" OFF)
option(SANDBOXCITY_ECS_DEMO "Activar modo ECS demo con 200 entidades" OFF)

if (SANDBOXCITY_KEEP_LEGACY_DRAWS)
  target_compile_definitions(SandboxCity PRIVATE SANDBOXCITY_KEEP_LEGACY_DRAWS=1)
endif()

if (SANDBOXCITY_ECS_DEMO)
  target_compile_definitions(SandboxCity PRIVATE SANDBOXCITY_ECS_DEMO=1)
endif()

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

target_link_directories(SandboxCity PRIVATE ${BULLET_LIBRARY_DIRS})

# Vincular librerías
target_link_libraries(SandboxCity PRIVATE
  bgfx::bgfx
  glfw
  tinyobjloader::tinyobjloader
  nlohmann_json::nlohmann_json
  BulletSoftBody
  BulletDynamics
  BulletCollision
  Bullet3Common
  LinearMath
)

# Salida por configuración (VS multi-config)
set_target_properties(SandboxCity PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)
