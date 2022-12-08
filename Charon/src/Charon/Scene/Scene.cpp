#include "pch.h"
#include "Scene.h"
#include "Charon/Scene/SceneObject.h"
#include "Charon/Scene/Components.h"
#include "Charon/Graphics/SceneRenderer.h"

namespace Charon {

	Scene::Scene()
	{
	}

	void Scene::Update()
	{
	}

	void Scene::Render(Ref<Camera> camera)
	{
		SceneRenderer::Begin(this, camera);

		auto group = m_Registry.group<MeshComponent>(entt::get<TransformComponent>);
		for (auto object : group)
		{
			auto [transform, mesh] = group.get<TransformComponent, MeshComponent>(object);
			//SceneRenderer::SubmitMesh(mesh.GetMesh(), transform.GetTransform());
		}

		SceneRenderer::End();
	}

	SceneObject Scene::CreateObject(const std::string& tag)
	{
		SceneObject sceneObject(m_Registry.create(), this);
		sceneObject.AddComponent<TagComponent>(tag);
		sceneObject.AddComponent<TransformComponent>();
		return sceneObject;
	}

	SceneObject Scene::CreateEmptyObject()
	{
		SceneObject sceneObject(m_Registry.create(), this);
		return sceneObject;
	}

	void Scene::RemoveObject(SceneObject& sceneObject)
	{
		m_Registry.destroy(sceneObject);
	}

}