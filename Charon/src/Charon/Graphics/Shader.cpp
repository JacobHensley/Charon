#include "pch.h"
#include "Shader.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/VulkanTools.h"
#include <shaderc/shaderc.hpp>
#include <spirv_cross.hpp>
#include <spirv_common.hpp>

namespace Charon {

	namespace Utils {

		static shaderc_shader_kind ShaderStageToShaderc(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::VERTEX:	return shaderc_vertex_shader;
				case ShaderStage::FRAGMENT: return shaderc_fragment_shader;
				case ShaderStage::COMPUTE:  return shaderc_compute_shader;
			}

			CR_ASSERT(false, "Unknown Type");
			return (shaderc_shader_kind)0;
		}

		static VkShaderStageFlagBits ShaderStageToVulkan(ShaderStage stage)
		{
			switch (stage)
			{
				case ShaderStage::VERTEX:	return VK_SHADER_STAGE_VERTEX_BIT;
				case ShaderStage::FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
				case ShaderStage::COMPUTE:  return VK_SHADER_STAGE_COMPUTE_BIT;
			}

			CR_ASSERT(false, "Unknown Type");
			return (VkShaderStageFlagBits)0;
		}

		static ShaderUniformType GetType(spirv_cross::SPIRType type)
		{
			spirv_cross::SPIRType::BaseType baseType = type.basetype;
		
			if (baseType == spirv_cross::SPIRType::Float)
			{
				if (type.columns == 1)
				{
					if (type.vecsize == 1)	    return ShaderUniformType::FLOAT;
					else if (type.vecsize == 2) return ShaderUniformType::FLOAT2;
					else if (type.vecsize == 3) return ShaderUniformType::FLOAT3;
					else if (type.vecsize == 4) return ShaderUniformType::FLOAT4;
				}
				else 
				{
					return ShaderUniformType::MAT4;
				}
			}
			else if (baseType == spirv_cross::SPIRType::Image)
			{
				if (type.image.dim == 1)	     return ShaderUniformType::TEXTURE_2D;
				else if (type.image.dim == 3)    return ShaderUniformType::TEXTURE_CUBE;
			}
			else if (baseType == spirv_cross::SPIRType::SampledImage)
			{
				if (type.image.dim == 1)		 return ShaderUniformType::TEXTURE_2D;
				else if (type.image.dim == 3)    return ShaderUniformType::TEXTURE_CUBE;
			}
			else if (baseType == spirv_cross::SPIRType::Int)
			{
				return ShaderUniformType::INT;
			}
			else if (baseType == spirv_cross::SPIRType::UInt)
			{
				return ShaderUniformType::UINT;
			}
			else if (baseType == spirv_cross::SPIRType::Boolean)
			{
				return ShaderUniformType::BOOL;
			}

			CR_ASSERT(false, "Unknown Type");
			return (ShaderUniformType)0;
		}

	}

	Shader::Shader(const std::string& path)
		: m_Path(path)
	{
		Init();
		CR_LOG_INFO("Initialized Vulkan shader: {0}", m_Path);
	}

	Shader::~Shader()
	{
		VkDevice logicalDevice = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		for (auto shaderStageInfo : m_ShaderCreateInfo)
		{
			vkDestroyShaderModule(logicalDevice, shaderStageInfo.module, nullptr);
		}

		for (int i = 0; i < m_DescriptorSetLayouts.size(); i++)
		{
			vkDestroyDescriptorSetLayout(logicalDevice, m_DescriptorSetLayouts[i], nullptr);
		}
	}

	void Shader::Init()
	{
		m_ShaderSrc = SplitShaders(m_Path);
		CR_ASSERT(m_ShaderSrc.size() >= 1, "Shader is empty or path is invalid");

		bool result = CompileShaders(m_ShaderSrc);
		CR_ASSERT(result, "Failed to initialize shader")

		CreateDescriptorSetLayouts();
	}

	bool Shader::CompileShaders(const std::unordered_map<ShaderStage, std::string>& shaderSrc)
	{
		// Setup compiler
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

		VkDevice logicalDevice = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		for (auto&& [stage, src] : m_ShaderSrc)
		{
			// Compile shader source and check for errors
			auto compilationResult = compiler.CompileGlslToSpv(src, Utils::ShaderStageToShaderc(stage), m_Path.c_str(), options);
			if (compilationResult.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				CR_LOG_ERROR("Warnings ({0}), Errors ({1}) \n{2}", compilationResult.GetNumWarnings(), compilationResult.GetNumErrors(), compilationResult.GetErrorMessage());
				return false;
			}

			const uint8_t* data = reinterpret_cast<const uint8_t*>(compilationResult.cbegin());
			const uint8_t* dataEnd = reinterpret_cast<const uint8_t*>(compilationResult.cend());
			uint32_t size = dataEnd - data;

			std::vector<uint32_t> spirv(compilationResult.cbegin(), compilationResult.cend());

			// Create shader module
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = size;
			createInfo.pCode = reinterpret_cast<const uint32_t*>(data);

			VkShaderModule shaderModule;
			VK_CHECK_RESULT(vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &shaderModule));

			// Create shader stage
			VkPipelineShaderStageCreateInfo shaderStageInfo{};
			shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageInfo.stage = Utils::ShaderStageToVulkan(stage);;
			shaderStageInfo.module = shaderModule;
			shaderStageInfo.pName = "main";

			// Save shader stage info
			m_ShaderCreateInfo.push_back(shaderStageInfo);
			ReflectShader(spirv, stage);
		}
	}

	void Shader::ReflectShader(const std::vector<uint32_t>& data, ShaderStage stage)
	{
		spirv_cross::Compiler compiler(data);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();

		// Get all uniform buffers
		for (const spirv_cross::Resource& resource : resources.uniform_buffers)
		{
			auto& bufferType = compiler.get_type(resource.base_type_id);
			int memberCount = bufferType.member_types.size();

			UniformBufferDescription& buffer = m_UniformBufferDescriptions.emplace_back();
			buffer.Name = resource.name;
			buffer.Size = compiler.get_declared_struct_size(bufferType);
			buffer.BindingPoint = compiler.get_decoration(resource.id, spv::DecorationBinding);
			buffer.DescriptorSetIndex = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);

			// Get all members of the uniform buffer
			for (int i = 0; i < memberCount; i++)
			{
				ShaderUniform uniform;
				uniform.Name = compiler.get_member_name(bufferType.self, i);
				uniform.Size = compiler.get_declared_struct_member_size(bufferType, i);
				uniform.Type = Utils::GetType(compiler.get_type(bufferType.member_types[i]));
				uniform.Offset = compiler.type_struct_member_offset(bufferType, i);

				buffer.Uniforms.push_back(uniform);
			}
		}

		// Get all storage buffers
		for (const spirv_cross::Resource& resource : resources.storage_buffers)
		{
			auto& bufferType = compiler.get_type(resource.base_type_id);
			int memberCount = bufferType.member_types.size();

			StorageBufferDescription& buffer = m_StorageBufferDescriptions.emplace_back();

			buffer.Name = resource.name;
			buffer.BindingPoint = compiler.get_decoration(resource.id, spv::DecorationBinding);
			buffer.DescriptorSetIndex = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
		}

		// Get all attributes
		if (stage == ShaderStage::VERTEX)
		{
			uint32_t offset = 0;
			for (const spirv_cross::Resource& resource : resources.stage_inputs)
			{
				auto& type = compiler.get_type(resource.base_type_id);

				ShaderAttribute& attribute = m_ShaderAttributeDescriptions.emplace_back();

				attribute.Name = resource.name;
				attribute.Location = compiler.get_decoration(resource.id, spv::DecorationLocation);
				attribute.Type = Utils::GetType(type);
				attribute.Size = GetTypeSize(attribute.Type);
				attribute.Offset = offset;

				offset += attribute.Size;
			}
		}

		// Get all sampled images in the shader
		for (auto& resource : resources.sampled_images)
		{
			auto& type = compiler.get_type(resource.base_type_id);

			ShaderResource& shaderResource = m_ShaderResourceDescriptions.emplace_back();

			shaderResource.Name = resource.name;
			shaderResource.BindingPoint = compiler.get_decoration(resource.id, spv::DecorationBinding);
			shaderResource.DescriptorSetIndex = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			shaderResource.Dimension = type.image.dim;
			shaderResource.Type = Utils::GetType(type);
		}
	}

	void Shader::CreateDescriptorSetLayouts()
	{
		VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

		std::unordered_map<int, std::vector<VkDescriptorSetLayoutBinding>> descriptorSetLayoutBindings;

		// Create uniform buffer layout bindings
		for (int i = 0; i < m_UniformBufferDescriptions.size(); i++)
		{
			VkDescriptorSetLayoutBinding layout{};

			layout.binding = m_UniformBufferDescriptions[i].BindingPoint;
			layout.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			layout.descriptorCount = 1;
			layout.stageFlags = VK_SHADER_STAGE_ALL;
			layout.pImmutableSamplers = nullptr;

			descriptorSetLayoutBindings[m_UniformBufferDescriptions[i].DescriptorSetIndex].push_back(layout);
		}

		// Create storage buffer layout bindings
		for (int i = 0; i < m_StorageBufferDescriptions.size(); i++)
		{
			VkDescriptorSetLayoutBinding layout{};

			layout.binding = m_StorageBufferDescriptions[i].BindingPoint;
			layout.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			layout.descriptorCount = 1;
			layout.stageFlags = VK_SHADER_STAGE_ALL;
			layout.pImmutableSamplers = nullptr;

			descriptorSetLayoutBindings[m_StorageBufferDescriptions[i].DescriptorSetIndex].push_back(layout);
		}

		// Create resource layout bindings
		for (int i = 0; i < m_ShaderResourceDescriptions.size(); i++)
		{
			VkDescriptorSetLayoutBinding layout{};

			layout.binding = m_ShaderResourceDescriptions[i].BindingPoint;
			layout.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			layout.descriptorCount = 1;
			layout.stageFlags = VK_SHADER_STAGE_ALL;
			layout.pImmutableSamplers = nullptr;

			descriptorSetLayoutBindings[m_ShaderResourceDescriptions[i].DescriptorSetIndex].push_back(layout);
		}

		// Use layout bindings to create descriptor set layouts
		int ID = 0;
		for (auto const& [descriptorSetIndex, layouts] : descriptorSetLayoutBindings)
		{
			VkDescriptorSetLayoutCreateInfo layoutInfo{};
			layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			layoutInfo.bindingCount = layouts.size();
			layoutInfo.pBindings = layouts.data();

			for (int i = 0; i < m_UniformBufferDescriptions.size(); i++)
			{
				if (m_UniformBufferDescriptions[i].DescriptorSetIndex == descriptorSetIndex)
				{
					m_UniformBufferDescriptions[i].Index = ID;
				}
			}

			for (int i = 0; i < m_StorageBufferDescriptions.size(); i++)
			{
				if (m_StorageBufferDescriptions[i].DescriptorSetIndex == descriptorSetIndex)
				{
					m_StorageBufferDescriptions[i].Index = ID;
				}
			}

			for (int i = 0; i < m_ShaderResourceDescriptions.size(); i++)
			{
				if (m_ShaderResourceDescriptions[i].DescriptorSetIndex == descriptorSetIndex)
				{
					m_ShaderResourceDescriptions[i].Index = ID;
				}
			}

			ID++;

			VkDescriptorSetLayout& descriptorSetLayout = m_DescriptorSetLayouts.emplace_back();
			VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout));
		}
	}

	uint32_t Shader::GetTypeSize(ShaderUniformType type)
	{
		switch (type)
		{
			case ShaderUniformType::BOOL:   return 4;
			case ShaderUniformType::INT:    return 4;
			case ShaderUniformType::UINT:   return 4;
			case ShaderUniformType::FLOAT:  return 4;
			case ShaderUniformType::FLOAT2: return 4 * 2;
			case ShaderUniformType::FLOAT3: return 4 * 3;
			case ShaderUniformType::FLOAT4: return 4 * 4;
			case ShaderUniformType::MAT4:   return 64;
		}

		CR_ASSERT(false, "Unknown Type");
		return 0;
	}

	std::unordered_map<ShaderStage, std::string> Shader::SplitShaders(const std::string& path)
	{
		std::unordered_map<ShaderStage, std::string> result;
		ShaderStage stage = ShaderStage::NONE;

		std::ifstream stream(path);
		
		std::stringstream ss[2];
		std::string line;

		while (getline(stream, line))
		{
			if (line.find("#Shader") != std::string::npos)
			{
				if (line.find("Vertex") != std::string::npos)
				{
					stage = ShaderStage::VERTEX;
				}
				else if (line.find("Fragment") != std::string::npos)
				{
					stage = ShaderStage::FRAGMENT;
				}
				else if (line.find("Compute") != std::string::npos)
				{
					stage = ShaderStage::COMPUTE;
				}
			}
			else
			{
				result[stage] += line + '\n';
			}
		}

		return result;
	}

}