#pragma once
#include "Charon/Scene/Scene.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Graphics/Mesh.h"
#include "Charon/Graphics/Framebuffer.h"
#include "glm/glm.hpp"

namespace Charon {

    class SceneRenderer
    {
    public:
        static void Init();

        static void Begin(Scene* scene, Ref<Camera> camera);
        static void End();

        static void SubmitMesh(Ref<Mesh> mesh, glm::mat4 transform);

        static Ref<Framebuffer> GetFinalBuffer();

    private:
        static void GeometryPass();

    };

}