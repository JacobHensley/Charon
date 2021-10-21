#pragma once
#include "Charon/Graphics/Image.h"
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace Charon {

	struct FramebufferSpecification
	{
		uint32_t Width = 0;
		uint32_t Height = 0;
		float Scale = 1.0f;
		std::vector<VkFormat> AttachmentFormats;
		glm::vec4 ClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	struct FramebufferAttachment
	{
		Ref<Image> Image;
		VkAttachmentDescription Description;
	};

	class Framebuffer
	{
	public:
		Framebuffer(const FramebufferSpecification& specification);
		~Framebuffer();

	public:
		void Resize(uint32_t width, uint32_t height);
		void Invalidate();

		inline VkFramebuffer GetFramebuffer() { return m_Framebuffer; }
		inline VkRenderPass GetRenderPass() { return m_RenderPass; }

		inline uint32_t GetWidth() { return m_Width; }
		inline uint32_t GetHeight() { return m_Height; }

		inline const FramebufferSpecification& GetSpecification() const { return m_Specification; }

		Ref<Image> GetImage(uint32_t index) { return m_Attachments[index].Image; }

	private:
		void Init();

	private:
		FramebufferSpecification m_Specification;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;

		VkFramebuffer m_Framebuffer = nullptr;
		VkRenderPass m_RenderPass = nullptr;

		std::vector<FramebufferAttachment> m_Attachments;
	};

}