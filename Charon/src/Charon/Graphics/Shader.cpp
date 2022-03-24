#include "pch.h"
#include "Shader.h"
#include "Charon/Core/Application.h"
#include "Charon/Graphics/VulkanTools.h"
#include <shaderc/shaderc.hpp>
#include <spirv_cross.hpp>
#include <spirv_common.hpp>
#include <codecvt>

namespace Charon {

	static IDxcCompiler3* s_HLSLCompiler;
	static IDxcUtils* s_HLSLUtils;
	static IDxcIncludeHandler* s_DefaultIncludeHandler;

	class CustomIncludeHandler : public IDxcIncludeHandler
	{
	public:
		CustomIncludeHandler()
		{
		}

		HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_result_maybenull_ IDxcBlob** ppIncludeSource) override
		{
			IDxcBlobEncoding* pEncoding;

			// NOTE: This is the worst
			std::wstring wstring(pFilename);
			using convert_type = std::codecvt_utf8<wchar_t>;
			std::wstring_convert<convert_type, wchar_t> converter;
			std::string path = converter.to_bytes(wstring);

			if (IncludedFiles.find(path) != IncludedFiles.end())
			{
				// Return empty string blob if this file has been included before
				static const char nullStr[] = " ";
				s_HLSLUtils->CreateBlob(nullStr, ARRAYSIZE(nullStr), CP_UTF8, &pEncoding);
				*ppIncludeSource = pEncoding;
				return S_OK;
			}

			HRESULT hr = s_HLSLUtils->LoadFile(pFilename, nullptr, &pEncoding);
			if (SUCCEEDED(hr))
			{
				IncludedFiles.insert(path);
				*ppIncludeSource = pEncoding;
			}
			else
			{
				*ppIncludeSource = nullptr;
			}
			return hr;
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, _COM_Outptr_ void __RPC_FAR* __RPC_FAR* ppvObject) override
		{
			return s_DefaultIncludeHandler->QueryInterface(riid, ppvObject);
		}

		ULONG STDMETHODCALLTYPE AddRef(void) override { return 0; }
		ULONG STDMETHODCALLTYPE Release(void) override { return 0; }

		std::unordered_set<std::string> IncludedFiles;
	};

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

		// TODO: Fix issue with structs
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

			return (ShaderUniformType)0;
		}

	}

	Shader::Shader(const std::string& path)
		: m_Path(path), m_EntryPoint("main"), m_Defines({})
	{
		Init();
		CR_LOG_INFO("Initialized Vulkan shader: {0}", m_Path);
	}

	Shader::Shader(std::string_view path, std::string_view entryPoint)
		: m_Path(path), m_EntryPoint(entryPoint), m_Defines({})
	{
		Init();
		CR_LOG_INFO("Initialized Vulkan shader: {0}", m_Path);
	}

	Shader::Shader(std::string_view path, std::string_view entryPoint, const std::vector<std::wstring>& defines)
		: m_Path(path), m_EntryPoint(entryPoint), m_Defines(defines)
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
		// TODO: Make this static
		DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&s_HLSLCompiler));
		DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&s_HLSLUtils));

		m_ShaderSrc = SplitShaders(m_Path);
		CR_ASSERT(m_ShaderSrc.size() >= 1, "Shader is empty or path is invalid");

		// TODO: Save path as std::filesystem::path
		std::filesystem::path path(m_Path);
		bool result = false;
		if (path.extension() == ".hlsl")
		{
			result = CompileHLSLShaders(m_ShaderSrc);
		}
		else
		{
			result = CompileGLSLShaders(m_ShaderSrc);
		}

		CR_ASSERT(result, "Failed to initialize shader")

		CreateDescriptorSetLayouts();
	}

	bool Shader::CompileGLSLShaders(const std::unordered_map<ShaderStage, std::string>& shaderSrc)
	{
		// Setup compiler
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

		for (auto&& [stage, src] : m_ShaderSrc)
		{
			// Compile shader source and check for errors
			auto compilationResult = compiler.CompileGlslToSpv(src, Utils::ShaderStageToShaderc(stage), m_Path.c_str(), options);
			if (compilationResult.GetCompilationStatus() != shaderc_compilation_status_success)
			{
				CR_LOG_ERROR("Warnings ({0}), Errors ({1}) \n{2}", compilationResult.GetNumWarnings(), compilationResult.GetNumErrors(), compilationResult.GetErrorMessage());
				return false;
			}

			// Format data
			const uint8_t* data = reinterpret_cast<const uint8_t*>(compilationResult.cbegin());
			const uint8_t* dataEnd = reinterpret_cast<const uint8_t*>(compilationResult.cend());
			uint32_t size = dataEnd - data;

			std::vector<uint32_t> spirv(compilationResult.cbegin(), compilationResult.cend());

			VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

			// Create shader module (TODO: Create function for this)
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = size;
			createInfo.pCode = spirv.data();

			VkShaderModule shaderModule;
			VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

			// Create shader stage
			VkPipelineShaderStageCreateInfo shaderStageInfo{};
			shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageInfo.stage = Utils::ShaderStageToVulkan(stage);;
			shaderStageInfo.module = shaderModule;
			shaderStageInfo.pName = "main";

			m_ShaderCreateInfo.push_back(shaderStageInfo);
			ReflectShader(spirv, stage);
		}

		return true;
	}

	bool Shader::CompileHLSLShaders(const std::unordered_map<ShaderStage, std::string>& shaderSrc)
	{
		std::vector<const wchar_t*> arguments;

		arguments.push_back(L"-spirv");
		arguments.push_back(L"-fspv-target-env=vulkan1.2");

		//Strip reflection data and pdbs (see later)
		arguments.push_back(L"-Qstrip_debug");
		arguments.push_back(L"-Qstrip_reflect");

		arguments.push_back(L"-I /assets/shaders/Sorting");

		arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS);
		arguments.push_back(DXC_ARG_DEBUG);
		arguments.push_back(DXC_ARG_PACK_MATRIX_COLUMN_MAJOR);

		for (const std::wstring& define : m_Defines)
		{
			arguments.push_back(L"-D");
			arguments.push_back(define.c_str());
		}

		for (const auto& [stage, shaderSrc] : shaderSrc)
		{
			std::wstring widestr = std::wstring(m_EntryPoint.begin(), m_EntryPoint.end());
			const wchar_t* widecstr = widestr.c_str();

			// Set entry point
			arguments.push_back(L"-E");
			arguments.push_back(widecstr);

			// Set stage target (TODO: Add more stage options)
			arguments.push_back(L"-T");
			arguments.push_back(L"cs_6_2");

			IDxcBlobEncoding* blobEncoding;
			s_HLSLUtils->CreateBlob(shaderSrc.c_str(), shaderSrc.size(), CP_UTF8, &blobEncoding);

			DxcBuffer sourceBuffer;
			sourceBuffer.Ptr = blobEncoding->GetBufferPointer();
			sourceBuffer.Size = blobEncoding->GetBufferSize();
			sourceBuffer.Encoding = 0;

			CustomIncludeHandler includeHandler = CustomIncludeHandler();

			IDxcResult* pCompileResult;
			s_HLSLCompiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), &includeHandler, IID_PPV_ARGS(&pCompileResult));

			IDxcBlobUtf8* pErrors;
			pCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
			if (pErrors && pErrors->GetStringLength() > 0)
			{
				CR_LOG_CRITICAL((char*)pErrors->GetBufferPointer());
				return false;
			}

			IDxcBlob* pResult;
			pCompileResult->GetResult(&pResult);

			size_t size = pResult->GetBufferSize();
			std::vector<uint32_t> spirv(size / sizeof(uint32_t));
			std::memcpy(spirv.data(), pResult->GetBufferPointer(), size);

			VkDevice device = Application::GetApp().GetVulkanDevice()->GetLogicalDevice();

			// Create shader module (TODO: Create function for this)
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = size;
			createInfo.pCode = spirv.data();

			VkShaderModule shaderModule;
			VK_CHECK_RESULT(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));

			// Create shader stage
			VkPipelineShaderStageCreateInfo shaderStageInfo{};
			shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStageInfo.stage = Utils::ShaderStageToVulkan(stage);;
			shaderStageInfo.module = shaderModule;
			shaderStageInfo.pName = m_EntryPoint.c_str();

			m_ShaderCreateInfo.push_back(shaderStageInfo);
			ReflectShader(spirv, stage);
		}

		return true;
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
			m_DescriptorSetLayoutMap[descriptorSetIndex] = descriptorSetLayout;
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

	// TODO: Pick a way to support HLSL stages
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