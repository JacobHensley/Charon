#pragma once
#include "Charon/Core/Core.h"
#include <entt/entt.hpp>

namespace Charon {

	class Scene;

	class SceneObject
	{
	public:
		SceneObject(entt::entity handle, Scene* scene)
			: m_Handle(handle), m_Scene(scene)
		{
		}

		SceneObject() = default;
		SceneObject(const SceneObject& other) = default;

	public:
		template<typename T, typename... Args>
		T& AddComponent(Args&&... args)
		{
			CR_ASSERT(!HasComponent<T>(), "Object already has this Component");
			return m_Scene->m_Registry.emplace<T>(m_Handle, std::forward<Args>(args)...);
		}

		template<typename T>
		bool HasComponent()
		{
			return m_Scene->m_Registry.any_of<T>(m_Handle);
		}

		template<typename T>
		T& GetComponent()
		{
			CR_ASSERT(HasComponent<T>(), "Object does not have this Component");
			return m_Scene->m_Registry.get<T>(m_Handle);
		}

		template<typename T>
		void RemoveComponent()
		{
			CR_ASSERT(HasComponent<T>(), "Object does not have this Component");
			m_Scene->m_Registry.remove<T>(m_Handle);
		}

		operator bool() const { return m_Handle != entt::null; }
		operator entt::entity() const { return m_Handle; }
		operator uint32_t() const { return (uint32_t)m_Handle; }

		bool operator==(const SceneObject& other) const
		{
			return m_Handle == other.m_Handle && m_Scene == other.m_Scene;
		}

		bool operator!=(const SceneObject& other) const
		{
			return !(*this == other);
		}

	private:
		entt::entity m_Handle = entt::null;
		Scene* m_Scene = nullptr;
	};

}