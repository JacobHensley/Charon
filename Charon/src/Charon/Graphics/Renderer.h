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

		void BeginRenderPass(Ref<Framebuffer> framebuffer = nullptr);
		void EndRenderPass();

		void SubmitMesh(Ref<Mesh> mesh, glm::mat4& transform);
		void Render();
		void RenderUI();

		void OnImGuiRender();

		Ref<Framebuffer> GetFramebuffer() { return m_Framebuffer; }

		static VkDescriptorSet AllocateDescriptorSet(VkDescriptorSetAllocateInfo allocInfo);

	private:
		void Init();
		void CreateDescriptorPools();
		std::vector<VkDescriptorSet> AllocateDescriptorSets(const std::vector<VkDescriptorSetLayout>& layouts);

	private:
		Ref<Camera> m_ActiveCamera;

		CameraBuffer m_CameraBuffer;
		std::vector<DrawCommand> m_DrawList;
		Ref<UniformBuffer> m_CameraUniformBuffer;
		Ref<Framebuffer> m_Framebuffer;
		Ref<Shader> m_Shader;

		Ref<VulkanPipeline> m_Pipeline;
		VkCommandBuffer m_ActiveCommandBuffer = nullptr;
		std::vector<VkDescriptorSet> m_DescriptorSets;
		std::vector<VkDescriptorPool> m_DescriptorPools;
	};

}
