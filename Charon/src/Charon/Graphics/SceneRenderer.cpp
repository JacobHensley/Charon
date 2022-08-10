#include "pch.h"
#include "SceneRenderer.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/Renderer.h"

namespace Charon {

	struct RenderCommand
	{
		Ref<Mesh> Mesh;
		glm::mat4 Transform;
	};

	struct SceneRendererData
	{
		Scene* ActiveScene = nullptr;
		Ref<Camera> ActiveCamera;
		std::vector<RenderCommand> RenderCommands;
	};

	static struct SceneRendererData s_Data;

	void SceneRenderer::Init()
	{
	}

	void SceneRenderer::Begin(Scene* scene, Ref<Camera> camera)
	{
		s_Data.ActiveScene = scene;
		s_Data.ActiveCamera = camera;

		Ref<Renderer> renderer = Application::GetApp().GetRenderer();
		renderer->BeginScene(s_Data.ActiveCamera);
	}

	void SceneRenderer::End()
	{
		// Render
		GeometryPass();

		Ref<Renderer> renderer = Application::GetApp().GetRenderer();
#if 0
		// UI
		renderer->BeginRenderPass();
		renderer->RenderUI();
		renderer->EndRenderPass();
#endif

		renderer->EndScene();

		s_Data.ActiveScene = nullptr;
		s_Data.ActiveCamera = nullptr;
		s_Data.RenderCommands.clear();
	}

	bool SceneRenderer::SetViewportSize(uint32_t width, uint32_t height)
	{
		Ref<Renderer> renderer = Application::GetApp().GetRenderer();
		return renderer->SetViewportSize(width, height);
	}

	void SceneRenderer::SubmitMesh(Ref<Mesh> mesh, glm::mat4 transform)
	{
		s_Data.RenderCommands.push_back(RenderCommand({ mesh, transform }));
	}

	void SceneRenderer::GeometryPass()
	{
		Ref<Renderer> renderer = Application::GetApp().GetRenderer();
		renderer->BeginRenderPass(renderer->GetFramebuffer(), true);

		for (RenderCommand command : s_Data.RenderCommands)
		{
			renderer->SubmitMesh(command.Mesh, command.Transform);
		}

		renderer->Render();
		renderer->EndRenderPass();
	}

	Ref<Framebuffer> SceneRenderer::GetFinalBuffer()
	{
		Ref<Renderer> renderer = Application::GetApp().GetRenderer();
		return renderer->GetFramebuffer();
	}

}