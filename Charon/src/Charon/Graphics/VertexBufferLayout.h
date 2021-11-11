#pragma once
#include "Shader.h"

namespace Charon {

	struct BufferElement
	{
		ShaderUniformType Type;
		uint32_t Offset;
	};

	class VertexBufferLayout
	{
	public:
		VertexBufferLayout(const std::vector<ShaderAttribute>& attributes);
		VertexBufferLayout(const std::initializer_list<BufferElement>& elements);

		inline const std::vector<VkVertexInputAttributeDescription>& GetVertexInputAttributes() { return m_VertexInputAttributes; }
		inline uint32_t GetStride() { return m_Stride; }
		inline void SetStride(uint32_t stride) { m_Stride = stride; }

	private:
		std::vector<ShaderAttribute> m_Attributes;
		std::vector<BufferElement> m_Elements;

		std::vector<VkVertexInputAttributeDescription> m_VertexInputAttributes;
		uint32_t m_Stride = 0;
	};

}