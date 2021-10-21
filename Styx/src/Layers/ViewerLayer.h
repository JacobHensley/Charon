#pragma once
#include "Charon/Core/Layer.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Graphics/Mesh.h"
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
		Ref<Mesh> m_Mesh;
		glm::mat4 m_MeshTransform;
	};

}