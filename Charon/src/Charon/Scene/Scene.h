#pragma once
#include "Charon/Scene/SceneObject.h"
#include "Charon/Graphics/Camera.h"
#include <entt/entt.hpp>

namespace Charon {

	class Scene
	{
	public:
		Scene();

	public:
		void Update();
		void Render(Ref<Camera> camera);

		SceneObject CreateObject(const std::string& tag);
		SceneObject CreateEmptyObject();
		void RemoveObject(SceneObject& sceneObject);

	private:
		entt::registry m_Registry;

		friend class SceneObject;
		friend class SceneSerializer;
	};
}