# SandboxCity

## Descripción General
SandboxCity es un motor de juego y entorno sandbox desarrollado en C++20. El proyecto está diseñado para ser modular y eficiente, utilizando una arquitectura **ECS (Entity Component System)** para la gestión de la lógica del juego y los objetos.

## Arquitectura

El proyecto se basa en varios pilares fundamentales:

### 1. Core (Núcleo)
El núcleo de la aplicación (`src/core`) gestiona el ciclo de vida del programa. La clase `Application` es el punto de entrada que inicializa la ventana, el renderer, la física y el bucle principal (Game Loop).

### 2. ECS (Entity Component System)
La lógica del juego reside en el directorio `src/ecs`.
- **Scene**: Actúa como el gestor principal del mundo, conteniendo todas las entidades y componentes.
- **Entidades**: Son identificadores únicos que representan objetos en el mundo.
- **Componentes**: Estructuras de datos puros que definen las propiedades de las entidades (e.g., `Transform`, `MeshRenderer`, `RigidBody`, `Collider`).
- **Sistemas**: Lógica que itera sobre las entidades con componentes específicos para actualizar su estado (e.g., `RenderSystem`, `PhysicsSystem`).

### 3. Renderizado (Rendering)
El sistema de renderizado (`src/render`) utiliza **bgfx**, una librería de renderizado agnóstica a la API, lo que permite soportar múltiples backends gráficos (DirectX, OpenGL, Vulkan, Metal).
- Soporta carga de modelos 3D y materiales.
- Gestiona cámaras y controladores de cámara.

### 4. Física (Physics)
La simulación física (`src/physics`) está impulsada por **Bullet Physics**.
- Soporta cuerpos rígidos (estáticos, dinámicos, cinemáticos).
- Gestión de colisiones y volúmenes de activación (triggers).
- Controladores de caracteres físicos.

## Estructura del Proyecto

```
SandboxCity/
├── src/
│   ├── core/       # Clases base de la aplicación (Application, Window, Time)
│   ├── ecs/        # Implementación del sistema de Entidades y Componentes
│   ├── render/     # Wrapper de bgfx y sistema de renderizado
│   ├── physics/    # Integración con Bullet Physics
│   ├── input/      # Sistema de entrada (Teclado/Mouse)
│   ├── scene/      # Gestión de escenas
│   ├── asset/      # Gestión de recursos
│   └── main.cpp    # Punto de entrada de la aplicación
├── assets/         # Recursos (modelos, texturas, shaders)
├── CMakeLists.txt  # Configuración de construcción con CMake
└── scripts/        # Scripts de utilidad
```

## Dependencias

El proyecto utiliza las siguientes librerías de terceros (gestionadas vía CMake/vcpkg):
- **bgfx**: Renderizado gráfico.
- **glfw3**: Creación de ventanas y gestión de contexto.
- **tinyobjloader**: Carga de modelos OBJ.
- **nlohmann_json**: Parseo de archivos JSON.
- **Bullet**: Motor de física.

## Requisitos de Compilación
- Compilador compatible con **C++20**.
- **CMake** 3.20 o superior.
