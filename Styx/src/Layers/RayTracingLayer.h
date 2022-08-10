#pragma once
#include "Charon/Core/Layer.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Asset/AssetManager.h"
#include "Charon/Scene/Scene.h"
#include <glm/gtc/type_ptr.hpp>

#include "Charon/Graphics/VulkanAccelerationStructure.h"
#include "UI/ViewportPanel.h"

namespace Charon {

	class RayTracingLayer : public Layer
	{
	public:
		RayTracingLayer();

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
		Ref<ViewportPanel> m_ViewportPanel;

		Ref<VulkanAccelerationStructure> m_AccelerationStructure;
	};

}