#pragma once

#include <glm/glm.hpp>

namespace Charon {

	struct MaterialBuffer
	{
		glm::vec3 AlbedoValue{0.8f};
		float Metallic = 0.0f;
		float Roughness = 1.0f;
	};

	class Material
	{
	public:
		Material() = default; // TODO: shader
		virtual ~Material() = default;

		MaterialBuffer& GetMaterialBuffer() { return m_MaterialBuffer; }
	private:
		MaterialBuffer m_MaterialBuffer;
	};


}
