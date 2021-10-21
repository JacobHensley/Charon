#include "pch.h"
#include "Scene.h"
#include "Charon/Scene/SceneObject.h"
#include "Charon/Scene/Components.h"

namespace Charon {

	Scene::Scene()
	{
	}

	void Scene::Update()
	{
	}

	void Scene::Render(Ref<Camera> camera)
	{
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