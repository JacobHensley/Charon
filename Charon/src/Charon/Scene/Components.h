#pragma once
#include "Charon/Asset/AssetManager.h"
#include "Charon/Graphics/Mesh.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <string>

namespace Charon {

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent&) = default;
		TagComponent(const std::string& tag)
			: Tag(tag)
		{
		}

		operator std::string& () { return Tag; }
		operator const std::string& () const { return Tag; }
	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f }; // Radians
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

		TransformComponent() = default;
		TransformComponent(const TransformComponent&) = default;

		glm::mat4 GetTransform() const
		{
			return glm::translate(glm::mat4(1.0f), Translation) * glm::toMat4(glm::quat(Rotation)) * glm::scale(glm::mat4(1.0f), Scale);
		}
	};

	struct MeshComponent
	{
		AssetHandle Mesh;

		MeshComponent() = default;
		MeshComponent(const MeshComponent&) = default;
		MeshComponent(AssetHandle mesh)
			: Mesh(mesh)
		{
		}

		Ref<Charon::Mesh> GetMesh() { return AssetManager::Get<Charon::Mesh>(Mesh); }
		UUID GetUUID() { return Mesh.GetUUID(); }
	};

}