#include "pch.h"
#include "VertexBufferLayout.h"

namespace Charon {

	namespace Utils {
	
		// Note: Does not support mat4 as that should be passed in a four vec4s also does not bool
		static VkFormat ShaderTypeToVkFormat(ShaderUniformType type)
		{
			switch (type)
			{
				case Charon::ShaderUniformType::INT:    return VK_FORMAT_R32_SINT;
				case Charon::ShaderUniformType::UINT:   return VK_FORMAT_R32_UINT;
				case Charon::ShaderUniformType::FLOAT:  return VK_FORMAT_R32_SFLOAT;
				case Charon::ShaderUniformType::FLOAT2: return VK_FORMAT_R32G32_SFLOAT;
				case Charon::ShaderUniformType::FLOAT3: return VK_FORMAT_R32G32B32_SFLOAT;
				case Charon::ShaderUniformType::FLOAT4: return VK_FORMAT_R32G32B32A32_SFLOAT;
			}

			CR_ASSERT(false, "Unknown Type");
			return (VkFormat)0;
		}

	}

	VertexBufferLayout::VertexBufferLayout(const std::vector<ShaderAttribute>& attributes)
		: m_Attributes(attributes)
	{
		m_VertexInputAttributes.resize(m_Attributes.size());
		for (uint32_t i = 0; i < m_Attributes.size(); i++)
		{
			// Note: Not sure how to handle binding for multiple vertex buffers
			m_VertexInputAttributes[i].binding = 0;
			m_VertexInputAttributes[i].location = m_Attributes[i].Location;
			m_VertexInputAttributes[i].format = Utils::ShaderTypeToVkFormat(m_Attributes[i].Type);
			m_VertexInputAttributes[i].offset = m_Attributes[i].Offset;

			m_Stride += Shader::GetTypeSize(m_Attributes[i].Type);
		}
	}

	VertexBufferLayout::VertexBufferLayout(const std::initializer_list<BufferElement>& elements)
		: m_Elements(elements)
	{
		m_VertexInputAttributes.resize(m_Elements.size());

		for (uint32_t i = 0; i < m_Elements.size(); i++)
		{
			uint32_t size = Shader::GetTypeSize(m_Elements[i].Type);

			// Note: Not sure how to handle binding for multiple vertex buffers
			m_VertexInputAttributes[i].binding = 0;
			m_VertexInputAttributes[i].location = i;
			m_VertexInputAttributes[i].format = Utils::ShaderTypeToVkFormat(m_Elements[i].Type);
			m_VertexInputAttributes[i].offset = m_Elements[i].Offset;

			m_Stride = m_Elements[i].Offset + size;
		}
	}

}