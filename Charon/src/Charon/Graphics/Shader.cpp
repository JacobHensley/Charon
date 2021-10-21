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

		// NOTE: This only takes in base type and size into account so arrays will be given the wrong type. If this becomes a issue we will change to vecsize.
		static ShaderUniformType GetUniformType(spirv_cross::SPIRType baseType, uint32_t size)
		{
			spirv_cross::SPIRType::BaseType type = baseType.basetype;

			if (type == spirv_cross::SPIRType::Float)
			{
				if (size == 4)			return ShaderUniformType::FLOAT;
				else if (size == 4 * 2) return ShaderUniformType::FLOAT2;
				else if (size == 4 * 3) return ShaderUniformType::FLOAT3;
				else if (size == 4 * 4) return ShaderUniformType::FLOAT4;
				else if (size == 64)	return ShaderUniformType::MAT4;
			}
			else if (type == spirv_cross::SPIRType::Int)	 return ShaderUniformType::INT;
			else if (type == spirv_cross::SPIRType::Boolean) return ShaderUniformType::BOOL;

			CR_ASSERT(false, "Unknown Type");
			return (ShaderUniformType)0;
		}

		static ShaderUniformType GetResourceType(spirv_cross::SPIRType baseType, uint32_t dimension)
		{
			spirv_cross::SPIRType::BaseType type = baseType.basetype;

			if (type == spirv_cross::SPIRType::Image)
			{
				if (dimension == 1)	     return ShaderUniformType::TEXTURE_2D;
				else if (dimension == 3) return ShaderUniformType::TEXTURE_CUBE;
			}
			else if (type == spirv_cross::SPIRType::SampledImage)
			{
				if (dimension == 1)		 return ShaderUniformType::TEXTURE_2D;
				else if (dimension == 3) return ShaderUniformType::TEXTURE_CUBE;
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
			ReflectShader(spirv);
		}
	}

	// TODO: Get info about push constants, other types of buffers and shader stages
	void Shader::ReflectShader(const std::vector<uint32_t>& data)
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
				uniform.Type = Utils::GetUniformType(compiler.get_type(bufferType.member_types[i]), uniform.Size);
				uniform.Offset = compiler.type_struct_member_offset(bufferType, i);

				buffer.Uniforms.push_back(uniform);
			}
		}

		// Get all sampled images in the shader
		for (auto& resource : resources.sampled_images)
		{
			auto& type = compiler.get_type(resource.base_type_id);

			ShaderResource& uniform = m_ShaderResourceDescriptions.emplace_back();

			uniform.Name = resource.name;
			uniform.BindingPoint = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uniform.DescriptorSetIndex = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uniform.Dimension = type.image.dim;
			uniform.Type = Utils::GetResourceType(type, uniform.Dimension);
		}

	}

	// TODO: Get info a shader stages so we don't have to use VK_SHADER_STAGE_ALL
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