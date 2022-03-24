#pragma once
#include "Charon/Asset/Asset.h"
#include <vulkan/vulkan.h>
#include <unknwn.h>
#include <combaseapi.h>
#include <dxc/dxcapi.h>

namespace Charon {

	enum class ShaderStage
	{
		NONE = -1, VERTEX, FRAGMENT, COMPUTE
	};

	enum class ShaderUniformType
	{
		NONE = -1, BOOL, INT, UINT, FLOAT, FLOAT2, FLOAT3, FLOAT4, MAT4, TEXTURE_2D, TEXTURE_CUBE, IMAGE_2D, IMAGE_CUBE
	};

	struct ShaderUniform
	{
		std::string Name;
		ShaderUniformType Type;
		uint32_t Size;
		uint32_t Offset;
	};

	struct ShaderAttribute
	{
		std::string Name;
		ShaderUniformType Type;
		uint32_t Location;
		uint32_t Size;
		uint32_t Offset;
	};

	struct ShaderResource
	{
		std::string Name;
		ShaderUniformType Type;
		uint32_t BindingPoint;
		uint32_t DescriptorSetIndex;
		uint32_t Index;
		uint32_t Dimension;
	};

	struct UniformBufferDescription
	{
		std::string Name;
		uint32_t Size;
		uint32_t BindingPoint;
		uint32_t DescriptorSetIndex;
		uint32_t Index;

		std::vector<ShaderUniform> Uniforms;
	};

	struct StorageBufferDescription
	{
		std::string Name;
		uint32_t BindingPoint;
		uint32_t DescriptorSetIndex;
		uint32_t Index;
	};

	class Shader : public Asset
	{
	public:
		Shader(const std::string& path);
		Shader(std::string_view path, std::string_view entryPoint);
		Shader(std::string_view path, std::string_view entryPoint, const std::vector<std::wstring>& defines);
		~Shader();

	public:
		inline const std::vector<UniformBufferDescription>& GetUniformBufferDescriptions() { return m_UniformBufferDescriptions; }
		inline const std::vector<StorageBufferDescription>& GetStorageBufferDescriptions() { return m_StorageBufferDescriptions; }
		inline const std::vector<ShaderAttribute>& GetShaderAttributeDescriptions() { return m_ShaderAttributeDescriptions; }
		inline const std::vector<ShaderResource>& GetShaderResourceDescriptions() { return m_ShaderResourceDescriptions; }

		inline const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() { return m_DescriptorSetLayouts; }
		inline const VkDescriptorSetLayout GetDescriptorSetLayout(uint32_t set) { return m_DescriptorSetLayoutMap.at(set); }
		inline const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderCreateInfo() { return m_ShaderCreateInfo; };

		static uint32_t GetTypeSize(ShaderUniformType type);

	private:
		void Init();
		bool CompileGLSLShaders(const std::unordered_map<ShaderStage, std::string>& shaderSrc);
		bool CompileHLSLShaders(const std::unordered_map<ShaderStage, std::string>& shaderSrc);
		void ReflectShader(const std::vector<uint32_t>& data, ShaderStage stage);
		void CreateDescriptorSetLayouts();
		void CreateShaderModules(ShaderStage stage, const uint32_t* data, uint32_t size, const std::string& entryPoint);
		std::unordered_map<ShaderStage, std::string> SplitShaders(const std::string& path);

	private:
		const std::string m_Path;
		const std::string m_EntryPoint;
		std::unordered_map<ShaderStage, std::string> m_ShaderSrc;

		const std::vector<std::wstring>& m_Defines;

		std::vector<UniformBufferDescription> m_UniformBufferDescriptions;
		std::vector<StorageBufferDescription> m_StorageBufferDescriptions;
		std::vector<ShaderAttribute> m_ShaderAttributeDescriptions;
		std::vector<ShaderResource> m_ShaderResourceDescriptions;

		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
		std::unordered_map<int, VkDescriptorSetLayout> m_DescriptorSetLayoutMap;
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderCreateInfo;
	};

}