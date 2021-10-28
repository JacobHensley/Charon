#include "pch.h"
#include "Renderer.h"
#include "Charon/Core/Application.h"
#include "Charon/ImGUI/imgui_impl_vulkan_with_textures.h"

namespace Charon {

	static Renderer* s_Instance = nullptr;

	Renderer::Renderer()
	{
		s_Instance = this;
		Init();
	}

	Renderer::~Renderer()
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		for (int i = 0; i < m_DescriptorPools.size(); i++)
		{
			vkDestroyDescriptorPool(device, m_DescriptorPools[i], nullptr);
		}
	}

	void Renderer::Init()
	{
		Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();

		FramebufferSpecification spec;
		spec.AttachmentFormats = { VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_D24_UNORM_S8_UINT };
		spec.Width = 1280;
		spec.Height = 720;
		m_Framebuffer = CreateRef<Framebuffer>(spec);

		m_CameraUniformBuffer = CreateRef<UniformBuffer>(nullptr, sizeof(CameraBuffer));

		std::vector<VkVertexInputAttributeDescription> vertexInputAttributes(4);

		// Vertex 0: Position
		vertexInputAttributes[0].binding = 0;
		vertexInputAttributes[0].location = 0;
		vertexInputAttributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributes[0].offset = 0;

		// Vertex 1: Normal
		vertexInputAttributes[1].binding = 0;
		vertexInputAttributes[1].location = 1;
		vertexInputAttributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributes[1].offset = 12;

		// Vertex 2: Tangent
		vertexInputAttributes[2].binding = 0;
		vertexInputAttributes[2].location = 2;
		vertexInputAttributes[2].format = VK_FORMAT_R32G32B32_SFLOAT;
		vertexInputAttributes[2].offset = 24;

		// Vertex 3: TextureCoords
		vertexInputAttributes[3].binding = 0;
		vertexInputAttributes[3].location = 3;
		vertexInputAttributes[3].format = VK_FORMAT_R32G32_SFLOAT;
		vertexInputAttributes[3].offset = 36;

		uint32_t stride = sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec3) + sizeof(glm::vec2);

		m_Shader = CreateRef<Shader>("assets/shaders/test.shader");
		m_Pipeline = CreateRef<VulkanPipeline>(m_Shader, m_Framebuffer->GetRenderPass(), vertexInputAttributes, stride);

		CreateDescriptorPools();
	}

	void Renderer::BeginFrame()
	{
		Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();
		uint32_t frameIndex = swapChain->GetCurrentBufferIndex();

		m_ActiveCommandBuffer = swapChain->GetCurrentCommandBuffer();

		VK_CHECK_RESULT(vkResetDescriptorPool(device, m_DescriptorPools[frameIndex], 0));

		const std::vector<VkDescriptorSetLayout>& layouts = m_Shader->GetDescriptorSetLayouts();
		m_DescriptorSets = AllocateDescriptorSets(m_Shader->GetDescriptorSetLayouts());

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0;
		beginInfo.pInheritanceInfo = nullptr;

		VK_CHECK_RESULT(vkBeginCommandBuffer(m_ActiveCommandBuffer, &beginInfo));
	}

	void Renderer::EndFrame()
	{
		VK_CHECK_RESULT(vkEndCommandBuffer(m_ActiveCommandBuffer));
	}

	void Renderer::BeginScene(Ref<Camera> camera)
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		m_ActiveCamera = camera;

		m_CameraBuffer.ViewProjection = m_ActiveCamera->GetViewProjection();
		m_CameraBuffer.InverseViewProjection = m_ActiveCamera->GetInverseVP();
		m_CameraUniformBuffer->UpdateBuffer(&m_CameraBuffer);

		UniformBufferDescription cameraBufferDescription = m_Shader->GetUniformBufferDescriptions()[0];

		VkWriteDescriptorSet cameraBufferWriteDescriptor = {};
		cameraBufferWriteDescriptor.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraBufferWriteDescriptor.descriptorCount = 1;
		cameraBufferWriteDescriptor.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraBufferWriteDescriptor.dstSet = m_DescriptorSets[cameraBufferDescription.Index];
		cameraBufferWriteDescriptor.dstBinding = cameraBufferDescription.BindingPoint;
		cameraBufferWriteDescriptor.pBufferInfo = &m_CameraUniformBuffer->getDescriptorBufferInfo();
		cameraBufferWriteDescriptor.pImageInfo = nullptr;

		VkWriteDescriptorSet writeDescriptors[1] = { cameraBufferWriteDescriptor };

		vkUpdateDescriptorSets(device, 1, writeDescriptors, 0, nullptr);
	}

	void Renderer::EndScene()
	{
		m_ActiveCamera = nullptr;
		m_DrawList.clear();
	}

	void Renderer::BeginRenderPass(Ref<Framebuffer> framebuffer)
	{
		VkRenderPass renderPass;
		VkFramebuffer vulkanFramebuffer;
		VkExtent2D extent;

		if (framebuffer)
		{
			vulkanFramebuffer = framebuffer->GetFramebuffer();
			renderPass = framebuffer->GetRenderPass();
			extent = { framebuffer->GetWidth(), framebuffer->GetHeight() };
		}
		else
		{
			Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();

			vulkanFramebuffer = swapChain->GetCurrentFramebuffer();
			renderPass = swapChain->GetRenderPass();
			extent = swapChain->GetExtent();
		}

		VkClearValue clearColor[2];
		clearColor[0].color = { 0.1f, 0.1f, 0.1f, 1.0f };
		clearColor[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = vulkanFramebuffer;
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = extent;
		renderPassInfo.clearValueCount = framebuffer ? 2 : 1;
		renderPassInfo.pClearValues = clearColor;

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)extent.width;
		viewport.height = (float)extent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		if (!framebuffer)
		{
			viewport.y = (float)extent.height;
			viewport.height = -(float)extent.height;
		}

		vkCmdSetViewport(m_ActiveCommandBuffer, 0, 1, &viewport);
		vkCmdBeginRenderPass(m_ActiveCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	}

	void Renderer::EndRenderPass()
	{
		vkCmdEndRenderPass(m_ActiveCommandBuffer);
	}

	void Renderer::SubmitMesh(Ref<Mesh> mesh, glm::mat4& transform)
	{
		for (const SubMesh& subMesh : mesh->GetSubMeshes())
		{
			m_DrawList.push_back({ subMesh, mesh->GetVertexBuffer(), mesh->GetIndexBuffer(), transform });
		}
	}

	void Renderer::Render()
	{
		Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();

		vkCmdBindPipeline(m_ActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipeline());
		for (const DrawCommand& command : m_DrawList)
		{
			VkDeviceSize offset = 0;
			VkBuffer vertexBuffer = command.VertexBuffer->GetBuffer();
			vkCmdBindVertexBuffers(m_ActiveCommandBuffer, 0, 1, &vertexBuffer, &offset);
			vkCmdBindIndexBuffer(m_ActiveCommandBuffer, command.IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

			vkCmdPushConstants(m_ActiveCommandBuffer, m_Pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &(command.Transform * command.SubMesh.Transform));
			vkCmdBindDescriptorSets(m_ActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipelineLayout(), 0, m_DescriptorSets.size(), m_DescriptorSets.data(), 0, nullptr);

			vkCmdDrawIndexed(m_ActiveCommandBuffer, command.SubMesh.IndexCount, 1, command.SubMesh.IndexOffset, command.SubMesh.VertexOffset, 0);
		}
	}

	void Renderer::RenderUI()
	{
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_ActiveCommandBuffer);
	}

	void Renderer::OnImGuiRender()
	{
	}

	void Renderer::CreateDescriptorPools()
	{
		Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		// Create descriptor pool for each frame in flight
		m_DescriptorPools.resize(swapChain->GetFramesInFlight());

		// Define max number of each descriptor for each descriptor set
		VkDescriptorPoolSize poolSizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 }
		};

		// Create descriptor pool
		VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
		descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolCreateInfo.flags = 0;
		descriptorPoolCreateInfo.maxSets = 1000;
		descriptorPoolCreateInfo.poolSizeCount = 3;
		descriptorPoolCreateInfo.pPoolSizes = poolSizes;

		for (auto& descriptorPool : m_DescriptorPools)
		{
			VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));
		}
	}

	std::vector<VkDescriptorSet> Renderer::AllocateDescriptorSets(const std::vector<VkDescriptorSetLayout>& layouts)
	{
		Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();
		uint32_t frameIndex = swapChain->GetCurrentBufferIndex();

		// Allocate descriptor sets from descriptor pool for current frame given the layout info
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorSetCount = layouts.size();
		allocInfo.pSetLayouts = layouts.data();
		allocInfo.descriptorPool = m_DescriptorPools[frameIndex];

		std::vector<VkDescriptorSet> result;
		result.resize(layouts.size());

		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, result.data()));
		return result;
	}

	VkDescriptorSet Renderer::AllocateDescriptorSet(VkDescriptorSetAllocateInfo allocInfo)
	{
		Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();
		uint32_t frameIndex = swapChain->GetCurrentBufferIndex();

		allocInfo.descriptorPool = s_Instance->m_DescriptorPools[frameIndex];

		VkDescriptorSet result;
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &result));
		return result;
	}
}