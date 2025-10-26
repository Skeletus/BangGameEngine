#pragma once

class Scene;
class Renderer;

class RenderSystem
{
public:
    static void Render(Scene& scene, Renderer& renderer);
};

