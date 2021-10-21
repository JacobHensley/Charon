#pragma once
#include "Charon/Core/Layer.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Asset/AssetManager.h"
#include "Charon/Scene/Scene.h"
#include <glm/gtc/type_ptr.hpp>

namespace Charon {

	class ViewerLayer : public Layer
	{
	public:
		ViewerLayer();

	public:
		void Init();

		void OnUpdate();
		void OnRender();
		void OnImGUIRender();

	private:
		Ref<Camera> m_Camera;
		Ref<Scene> m_Scene;
		SceneObject m_SceneObject;
		AssetHandle m_MeshHandle;
	};

}