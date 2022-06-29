#include "pch.h"
#include "Framebuffer.h"
#include "Charon/Core/Application.h"

#include "Charon/Core/Log.h"

namespace Charon {

	Framebuffer::Framebuffer(const FramebufferSpecification& specification)
		:	m_Specification(specification)
	{
		Init();
		Resize(m_Specification.Width, m_Specification.Height);
	}

	Framebuffer::~Framebuffer()
	{
		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		vkDestroyRenderPass(device->GetLogicalDevice(), m_RenderPass, nullptr);
		vkDestroyFramebuffer(device->GetLogicalDevice(), m_Framebuffer, nullptr);
	}
	
	void Framebuffer::Init()
	{
		for (int i = 0; i < m_Specification.AttachmentFormats.size(); i++)
		{
			bool isDepth = Image::IsDepthFormat(m_Specification.AttachmentFormats[i]);
			FramebufferAttachment& attachment = isDepth ? m_DepthAttachment : m_Attachments.emplace_back();

			ImageSpecification imageSpecification = {};
			imageSpecification.Data = nullptr;
			imageSpecification.Width = m_Specification.Width;
			imageSpecification.Height = m_Specification.Height;
			imageSpecification.Format = m_Specification.AttachmentFormats[i];
			imageSpecification.UseStagingBuffer = false;
			imageSpecification.Usage = isDepth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			imageSpecification.DebugName = fmt::format("{}-{}({})", m_Specification.DebugName, isDepth ? "Depth" : "Color", i);
			attachment.Image = CreateRef<Image>(imageSpecification);

			// Fill attachment description
			attachment.Description = {};
			attachment.Description.samples = VK_SAMPLE_COUNT_1_BIT;
			attachment.Description.loadOp = m_Specification.ClearOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.Description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachment.Description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachment.Description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			attachment.Description.format = m_Specification.AttachmentFormats[i];
			attachment.Description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			// Final layout
			if (isDepth || Image::IsStencilFormat(m_Specification.AttachmentFormats[i]))
			{
				attachment.Description.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
			}
			else
			{
				attachment.Description.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
				m_ColorAttachmentCount++;
			}
		}
	}

	void Framebuffer::Resize(uint32_t width, uint32_t height)
	{
		width *= m_Specification.Scale;
		height *= m_Specification.Scale;

		if (m_Width == width && m_Height == height)
			return;

		m_Width = width;
		m_Height = height;

		Invalidate();
	}

	void Framebuffer::Invalidate()
	{
		// Collect attachment description
		std::vector<VkAttachmentDescription> attachmentDescriptions;
		for (auto& attachment : m_Attachments)
		{
			attachmentDescriptions.push_back(attachment.Description);
		};

		if (m_DepthAttachment)
			attachmentDescriptions.push_back(m_DepthAttachment.Description);

		// collect attachment references
		std::vector<VkAttachmentReference> colorAttachmentReferences;
		VkAttachmentReference depthAttachmentReference = {};

		bool hasDepth = false;
		bool hasColor = false;

		for (uint32_t i = 0; i < m_Attachments.size(); i++)
		{
			FramebufferAttachment attachment = m_Attachments[i];
			colorAttachmentReferences.push_back({ i, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
			hasColor = true;
		}

		if (m_DepthAttachment)
		{
			depthAttachmentReference.attachment = (uint32_t)m_Attachments.size();
			depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			hasDepth = true;
		}

		// Create suppass using attachment references
		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		if (hasColor)
		{
			subpass.pColorAttachments = colorAttachmentReferences.data();
			subpass.colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size());
		}
		if (hasDepth)
		{
			subpass.pDepthStencilAttachment = &depthAttachmentReference;
		}

		// Create subpass dependencies (Somthing to do with layout transitions)
		std::array<VkSubpassDependency, 2> dependencies;

		dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[0].dstSubpass = 0;
		dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		dependencies[1].srcSubpass = 0;
		dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		Ref<VulkanDevice> device = Application::GetApp().GetVulkanDevice();

		// Create render pass
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.pAttachments = attachmentDescriptions.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescriptions.size());
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();
		VK_CHECK_RESULT(vkCreateRenderPass(device->GetLogicalDevice(), &renderPassInfo, nullptr, &m_RenderPass));

		// Collect attachment image views
		std::vector<VkImageView> attachmentViews;
		for (auto attachment : m_Attachments)
		{
			attachmentViews.push_back(attachment.Image->GetDescriptorImageInfo().imageView);
		}

		if (m_DepthAttachment)
			attachmentViews.push_back(m_DepthAttachment.Image->GetDescriptorImageInfo().imageView);

		// Find max number of layers across attachments
		uint32_t maxLayers = 0;

		for (auto attachment : m_Attachments)
		{
			if (attachment.Image->GetSpecification().LayerCount > maxLayers)
			{
				maxLayers = attachment.Image->GetSpecification().LayerCount;
			}
		}
		
		// Create framebuffer
		VkFramebufferCreateInfo framebufferInfo = {};
		framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferInfo.renderPass = m_RenderPass;
		framebufferInfo.pAttachments = attachmentViews.data();
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
		framebufferInfo.width = m_Width;
		framebufferInfo.height = m_Height;
		framebufferInfo.layers = maxLayers;
		VK_CHECK_RESULT(vkCreateFramebuffer(device->GetLogicalDevice(), &framebufferInfo, nullptr, &m_Framebuffer));
	}

}