#pragma once
#include "Charon/Asset/Asset.h"
#include "Charon/Graphics/Buffers.h"
#include <tinygltf/tiny_gltf.h>
#include <glm/glm.hpp>

namespace Charon {

	struct SubMesh
	{
		uint32_t VertexOffset = 0;
		uint32_t IndexOffset = 0;
		uint32_t IndexCount = 0;
		glm::mat4 Transform = glm::mat4(1.0f);
	};

	struct Vertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec3 Tangent;
		glm::vec2 TextureCoords;
	};

	class Mesh : public Asset
	{
	public:
		Mesh(const std::string& path);
		~Mesh();

		inline const std::vector<SubMesh>& GetSubMeshes() const { return m_SubMeshes; }

		inline Ref<VertexBuffer> GetVertexBuffer() const { return m_VertexBuffer; }
		inline Ref<IndexBuffer> GetIndexBuffer() const { return m_IndexBuffer; }

	private:
		void Init();
		void LoadData();
		void CalculateNodeTransforms(const tinygltf::Node& inputNode, const tinygltf::Model& input, const glm::mat4& parentTransform);

	private:
		std::string m_Path;

		std::vector<SubMesh> m_SubMeshes;
		std::vector<Vertex> m_Vertices;
		std::vector<uint16_t> m_Indices;

		Ref<VertexBuffer> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;

		tinygltf::Model m_Model;
	};

}


