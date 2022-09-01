#include "pch.h"
#include "Renderer.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/VertexBufferLayout.h"
#include "Charon/ImGUI/imgui_impl_vulkan_with_textures.h"

#include <glm/gtc/type_ptr.hpp>

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

		m_CameraUniformBuffer = CreateRef<UniformBuffer>(nullptr, sizeof(CameraBuffer));
		m_Shader = CreateRef<Shader>("assets/shaders/test.shader");

		FramebufferSpecification framebufferSpec;
		framebufferSpec.AttachmentFormats = { VK_FORMAT_R32G32B32A32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT };
		framebufferSpec.Width = 1280;
		framebufferSpec.Height = 720;
		framebufferSpec.ClearOnLoad = false;
		framebufferSpec.DebugName = "Geometry";
		m_Framebuffer = CreateRef<Framebuffer>(framebufferSpec);

		PipelineSpecification pipelineSpec;
		pipelineSpec.Shader = m_Shader;
		pipelineSpec.TargetRenderPass = m_Framebuffer->GetRenderPass();
		m_Pipeline = CreateRef<VulkanPipeline>(pipelineSpec);
		
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
		m_CameraBuffer.View = m_ActiveCamera->GetViewMatrix();
		m_CameraBuffer.InverseView = m_ActiveCamera->GetInverseViewMatrix();
		m_CameraBuffer.InverseProjection = glm::inverse(m_ActiveCamera->GetProjectionMatrix());
		m_CameraUniformBuffer->UpdateBuffer(&m_CameraBuffer);

		if (false)
		{
			std::cout << '\n';
			std::cout << "View:\n";
			glm::mat4 iv = glm::inverse(m_CameraBuffer.View);
			float* vp = glm::value_ptr(iv);
			for (int x = 0; x < 4; x++)
			{
				for (int y = 0; y < 4; y++)
				{
					std::cout << vp[x + y * 4] << " | ";
				}
				std::cout << '\n';
			}
			std::cout << '\n';
		}
		if (false)
		{
			std::cout << '\n';
			std::cout << "View Projection:\n";
			float* vp = glm::value_ptr(m_CameraBuffer.ViewProjection);
			for (int x = 0; x < 4; x++)
			{
				for (int y = 0; y < 4; y++)
				{
					std::cout << vp[x + y * 4] << " | ";
				}
				std::cout << '\n';
			}
			std::cout << '\n';
		}

#if 0
		glm::vec3 p = glm::vec3(0.0f, 2.0f, 0.0f);
		glm::vec4 pos2D = m_CameraBuffer.ViewProjection * glm::vec4(p, 1.0f);
		glm::vec3 pos2D2 = glm::vec3(pos2D) / pos2D.w;
		std::cout << "W: " << pos2D.w << std::endl;
		std::cout << "X: " << pos2D2.x << ", Y: " << pos2D2.y << std::endl;
#endif

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

	void Renderer::BeginRenderPass(Ref<Framebuffer> framebuffer, bool explicitClear)
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

		VkRect2D scissor;
		scissor.offset = renderPassInfo.renderArea.offset;
		scissor.extent = renderPassInfo.renderArea.extent;

		vkCmdSetScissor(m_ActiveCommandBuffer, 0, 1, &scissor);
		vkCmdBeginRenderPass(m_ActiveCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		if (explicitClear)
		{
			const uint32_t colorAttachmentCount = (uint32_t)framebuffer->GetColorAttachmentCount();
			const uint32_t totalAttachmentCount = colorAttachmentCount + (framebuffer->HasDepthAttachment() ? 1 : 0);

			VkClearValue clearValues;
			glm::vec4 clearColor = framebuffer->GetSpecification().ClearColor;
			clearValues.color = { clearColor.r, clearColor.g, clearColor.b, clearColor.a };

			std::vector<VkClearAttachment> attachments(totalAttachmentCount);
			std::vector<VkClearRect> clearRects(totalAttachmentCount);
			for (uint32_t i = 0; i < colorAttachmentCount; i++)
			{
				attachments[i].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				attachments[i].colorAttachment = i;
				attachments[i].clearValue = clearValues;

				clearRects[i].rect.offset = { (int32_t)0, (int32_t)0 };
				clearRects[i].rect.extent = { extent.width, extent.height };
				clearRects[i].baseArrayLayer = 0;
				clearRects[i].layerCount = 1;
			}

			if (framebuffer->HasDepthAttachment())
			{
				clearValues.depthStencil = { 1.0f, 0 };

				attachments[colorAttachmentCount].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
				attachments[colorAttachmentCount].clearValue = clearValues;
				clearRects[colorAttachmentCount].rect.offset = { (int32_t)0, (int32_t)0 };
				clearRects[colorAttachmentCount].rect.extent = { extent.width,  extent.height };
				clearRects[colorAttachmentCount].baseArrayLayer = 0;
				clearRects[colorAttachmentCount].layerCount = 1;
			}

			vkCmdClearAttachments(m_ActiveCommandBuffer, totalAttachmentCount, attachments.data(), totalAttachmentCount, clearRects.data());
		}
	}

	void Renderer::EndRenderPass()
	{
		vkCmdEndRenderPass(m_ActiveCommandBuffer);
	}

	void Renderer::SubmitMesh(Ref<Mesh> mesh, const glm::mat4& transform)
	{
		for (const SubMesh& subMesh : mesh->GetSubMeshes())
		{
			m_DrawList.push_back({ subMesh, mesh->GetVertexBuffer(), mesh->GetIndexBuffer(), transform * subMesh.Transform });
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

			vkCmdPushConstants(m_ActiveCommandBuffer, m_Pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &command.Transform);
			vkCmdBindDescriptorSets(m_ActiveCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline->GetPipelineLayout(), 0, m_DescriptorSets.size(), m_DescriptorSets.data(), 0, nullptr);

			vkCmdDrawIndexed(m_ActiveCommandBuffer, command.SubMesh.IndexCount, 1, command.SubMesh.IndexOffset, command.SubMesh.VertexOffset, 0);
		}

		m_DrawList.clear();
	}

	void Renderer::RenderUI()
	{
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_ActiveCommandBuffer);
	}

	bool Renderer::SetViewportSize(uint32_t width, uint32_t height)
	{
		if (width == m_ViewportWidth && height == m_ViewportHeight)
			return false;

		m_ViewportWidth = width;
		m_ViewportHeight = height;

		FramebufferSpecification spec = m_Framebuffer->GetSpecification();
		spec.Width = width;
		spec.Height = height;
		m_Framebuffer = CreateRef<Framebuffer>(spec);
		return true;
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

	VkDescriptorSet Renderer::AllocateDescriptorSet(VkDescriptorSetLayout descLayout)
	{
		Ref<SwapChain> swapChain = Application::GetApp().GetVulkanSwapChain();
		uint32_t frameIndex = swapChain->GetCurrentBufferIndex();
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		VkDescriptorSetAllocateInfo allocInfo;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pNext = NULL;
		allocInfo.descriptorPool = s_Instance->m_DescriptorPools[frameIndex];
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &descLayout;

		VkDescriptorSet descriptorSet;
		VkResult res = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
		return descriptorSet;
	}

}