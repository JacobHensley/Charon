#pragma once
#include "Charon/Graphics/Camera.h"
#include "Charon/Graphics/Framebuffer.h"
#include "Charon/Graphics/VulkanPipeline.h"
#include "Charon/Graphics/Buffers.h"
#include "Charon/Graphics/Shader.h"
#include "Charon/Graphics/Mesh.h"

namespace Charon {

	struct CameraBuffer
	{
		glm::mat4 ViewProjection;
		glm::mat4 InverseViewProjection;
		glm::mat4 View;
		glm::mat4 InverseView;
		glm::mat4 InverseProjection;
	};

	struct DrawCommand
	{
		SubMesh SubMesh;
		Ref<VertexBuffer> VertexBuffer;
		Ref<IndexBuffer> IndexBuffer;

		glm::mat4 Transform;
	};

	class Renderer
	{
	public:
		Renderer();
		~Renderer();

	public:
		void BeginFrame();
		void EndFrame();

		void BeginScene(Ref<Camera> camera);
		void EndScene();

		void BeginRenderPass(Ref<Framebuffer> framebuffer = nullptr, bool explicitClear = false);
		void EndRenderPass();

		void SubmitMesh(Ref<Mesh> mesh, const glm::mat4& transform);
		void Render();
		void RenderUI();

		bool SetViewportSize(uint32_t width, uint32_t height);

		void OnImGuiRender();

		Ref<VulkanPipeline> GetPipeline() { return m_Pipeline; }
		std::vector<VkDescriptorSet> GetDescriptorSets() { return m_DescriptorSets; }
		void SetActiveCommandBuffer(VkCommandBuffer commandBuffer) { m_ActiveCommandBuffer = commandBuffer; }
		VkCommandBuffer GetActiveCommandBuffer() { return m_ActiveCommandBuffer; }
		uint32_t GetCurrentBufferIndex() const;

		Ref<Framebuffer> GetFramebuffer() { return m_Framebuffer; }

		static VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetAllocateInfo allocInfo);
		static VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetLayout descLayout);

		Ref<UniformBuffer> GetCameraUB() { return m_CameraUniformBuffer; }

		template<typename Fn>
		void SubmitResourceFree(Fn&& function)
		{
			uint32_t index = GetCurrentBufferIndex();
			m_ResourceFreeQueue[index].emplace_back(function);
		}
	private:
		void Init();
		void CreateDescriptorPools();
		std::vector<VkDescriptorSet> AllocateDescriptorSets(const std::vector<VkDescriptorSetLayout>& layouts);

	private:
		Ref<Camera> m_ActiveCamera;

		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		CameraBuffer m_CameraBuffer;
		std::vector<DrawCommand> m_DrawList;
		Ref<UniformBuffer> m_CameraUniformBuffer;
		Ref<Framebuffer> m_Framebuffer;
		Ref<Shader> m_Shader;

		Ref<VulkanPipeline> m_Pipeline;
		VkCommandBuffer m_ActiveCommandBuffer = nullptr;
		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::vector<VkDescriptorPool> m_DescriptorPools;

		std::vector<std::vector<std::function<void()>>> m_ResourceFreeQueue;
	};

}
