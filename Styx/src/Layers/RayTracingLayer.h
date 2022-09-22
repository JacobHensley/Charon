#pragma once
#include "Charon/Core/Layer.h"
#include "Charon/Graphics/Camera.h"
#include "Charon/Graphics/Image.h"
#include "Charon/Asset/AssetManager.h"
#include "Charon/Scene/Scene.h"
#include <glm/gtc/type_ptr.hpp>

#include "Charon/Graphics/VulkanAccelerationStructure.h"
#include "Charon/Graphics/VulkanRayTracingPipeline.h"
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

		void RayTracingPass();
	private:
		bool CreateRayTracingPipeline();
	private:
		Ref<Camera> m_Camera;
		Ref<Scene> m_Scene;
		SceneObject m_SceneObject;
		AssetHandle m_MeshHandle;
		Ref<ViewportPanel> m_ViewportPanel;

		struct SceneBuffer
		{
			glm::vec3 DirectionalLight_Direction;
			float Padding0;
			glm::vec3 PointLight_Position;
			float Padding1;
		} m_SceneBuffer;
		Ref<UniformBuffer> m_SceneUB;

		uint32_t m_RTWidth = 0, m_RTHeight = 0;

		Ref<VulkanAccelerationStructure> m_AccelerationStructure;
		Ref<VulkanRayTracingPipeline> m_RayTracingPipeline;
		Ref<Image> m_Image;
		std::vector<VkWriteDescriptorSet> m_RayTracingWriteDescriptors;
	};

}