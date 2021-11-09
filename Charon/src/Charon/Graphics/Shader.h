#pragma once
#include "Charon/Asset/Asset.h"
#include <vulkan/vulkan.h>

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
		~Shader();

	public:
		inline const std::vector<UniformBufferDescription>& GetUniformBufferDescriptions() { return m_UniformBufferDescriptions; }
		inline const std::vector<StorageBufferDescription>& GetStorageBufferDescriptions() { return m_StorageBufferDescriptions; }
		inline const std::vector<ShaderAttribute>& GetShaderAttributeDescriptions() { return m_ShaderAttributeDescriptions; }
		inline const std::vector<ShaderResource>& GetShaderResourceDescriptions() { return m_ShaderResourceDescriptions; }

		inline const std::vector<VkDescriptorSetLayout>& GetDescriptorSetLayouts() { return m_DescriptorSetLayouts; }
		inline const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderCreateInfo() { return m_ShaderCreateInfo; };

	private:
		void Init();
		bool CompileShaders(const std::unordered_map<ShaderStage, std::string>& shaderSrc);
		void ReflectShader(const std::vector<uint32_t>& data, ShaderStage stage);
		void CreateDescriptorSetLayouts();
		std::unordered_map<ShaderStage, std::string> SplitShaders(const std::string& path);

	private:
		const std::string m_Path;
		std::unordered_map<ShaderStage, std::string> m_ShaderSrc;

		std::vector<UniformBufferDescription> m_UniformBufferDescriptions;
		std::vector<StorageBufferDescription> m_StorageBufferDescriptions;
		std::vector<ShaderAttribute> m_ShaderAttributeDescriptions;
		std::vector<ShaderResource> m_ShaderResourceDescriptions;

		std::vector<VkDescriptorSetLayout> m_DescriptorSetLayouts;
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderCreateInfo;
	};

}