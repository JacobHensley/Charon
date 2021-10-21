#pragma once
#include "Charon/Core/Layer.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Asset/AssetManager.h"
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
		AssetHandle m_MeshHandle;
		glm::mat4 m_MeshTransform;
	};

}