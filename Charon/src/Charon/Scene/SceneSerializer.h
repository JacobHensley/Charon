#pragma once
#include "Charon/Core/Core.h"
#include "Charon/Scene/Scene.h"
#include <yaml-cpp/yaml.h>
#include <String>

namespace Charon
{
	class SceneSerializer
	{
	public:
		static void Save(Ref<Scene>& scene, const std::string& path);
		static Ref<Scene> Load(const std::string& path);

	private:
		static void SerializeAssets(YAML::Emitter& out);
		static void SerializeObjects(Ref<Scene>& scene, YAML::Emitter& out);
		static void SerializeObject(YAML::Emitter& out, SceneObject object);

		static void LoadAssets(YAML::Node assets);
		static void LoadObjects(Ref<Scene>& scene, YAML::Node object);
	};
}