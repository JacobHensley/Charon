#pragma once
#include "Asset.h"
#include "Charon/Graphics/Mesh.h"
#include "Charon/Graphics/Texture2D.h"
#include "Charon/Graphics/Shader.h"
#include <filesystem>

namespace Charon {

	class AssetManager
	{
		inline static std::unordered_map<UUID, Ref<Asset>, UUIDHash> m_Assets;		// Map of UUID to assets
		inline static std::unordered_map<UUID, std::string, UUIDHash> m_AssetPaths; // Map of UUIDs to absolute paths
		inline static std::unordered_map<std::string, UUID> m_Paths;				// Map of absolute paths to UUIDs

	public:

		// Returns an assets of type T given a handle if it exist in the system else asserts
		template<typename T>
		static Ref<T> Get(AssetHandle handle)
		{
			CR_ASSERT(m_Assets.find(handle) != m_Assets.end(), "Asset not found!");
			return std::dynamic_pointer_cast<T>(m_Assets.at(handle));
		}

		// Returns an assets of type T given a handle if it exist in the system else returns nullptr
		template<typename T>
		static Ref<T> TryGet(AssetHandle handle)
		{
			if (m_Assets.find(handle) != m_Assets.end())
			{
				return Get<T>(handle);
			}
				
			return nullptr;
		}

		// Loads an asset of type if is not in the system else asserts
		template<typename T>
		static AssetHandle Load(const std::string& path)
		{
			std::string absolutePath = GetAbsolutePath(path);
			CR_ASSERT(m_Paths.find(absolutePath) == m_Paths.end(), "Asset is already loaded");

			UUID uuid; // Generate a new UUID for the asset
			return InsertAndLoad<T>(uuid, absolutePath);
		}

		// Loads an asset of type if is not in the system else returns existing handle
		template<typename T>
		static AssetHandle TryLoad(const std::string& path)
		{
			std::string absolutePath = GetAbsolutePath(path);

			if (m_Paths.find(absolutePath) != m_Paths.end())
			{
				AssetHandle handle = (m_Paths[absolutePath]);
				CR_LOG_TRACE("Found existing asset [{0}] ({1})", (uint64_t)handle.GetUUID(), path);
				return handle;
			}

			UUID uuid; // Generate a new UUID for the asset
			return InsertAndLoad<T>(uuid, absolutePath);
		}

		// If the UUID already exist in the system then a assert is hit
		// If the asset path is found in the system then it inserts that same asset back into the system with a new path and UUID
		// If nothing is found in the system then the asset is loaded from disk and inserted into the system
		// Returns a UUID to the asset
		template<typename T>
		static AssetHandle InsertAndLoad(UUID uuid, const std::string& path)
		{
			CR_ASSERT(m_Assets.find(uuid) == m_Assets.end(), "Asset UUID already exists in map!");

			AssetHandle handle;
			std::string absolutePath = GetAbsolutePath(path);
			
			if (m_Paths.find(absolutePath) != m_Paths.end())
			{
				handle = AssetHandle(m_Paths[absolutePath]);
				Insert(m_Assets[handle.GetUUID()], path, uuid);
				CR_LOG_TRACE("Found existing asset [{0}], inserting as [{1}] ({2})", (uint64_t)handle.GetUUID(), (uint64_t)uuid, path);
			}
			else
			{
				Insert(LoadAsset<T>(path), path, uuid);
			}

			return uuid;
		}

		// Returns the asset with the specified UUID and if that UUID is not found then it InsertAndLoads the asset
		template<typename T>
		static AssetHandle TryInsertAndLoad(UUID uuid, const std::string& path)
		{
			if (m_Assets.find(uuid) != m_Assets.end())
				return uuid;

			return InsertAndLoad<T>(uuid, path);
		}

		// Inserts an asset into the system with no checks as to whether or not an assets exists in the slot already
		static AssetHandle Insert(Ref<Asset> asset, const std::string& path, UUID uuid)
		{
			CR_ASSERT(!path.empty(), "Path cannot be empty");

			std::string absolutePath = GetAbsolutePath(path);

			m_Assets[uuid] = asset;
			m_AssetPaths[uuid] = absolutePath;
			m_Paths[absolutePath] = uuid;
			return uuid;
		}

		// Clears the entire system intended for use on application shutdown
		static void Clear()
		{
			m_Assets.clear();
			m_AssetPaths.clear();
			m_Paths.clear();
		}

	public:
		static const std::unordered_map<UUID, Ref<Asset>, UUIDHash>& GetAssets()
		{
			return m_Assets;
		}

		static const std::unordered_map<UUID, std::string, UUIDHash>& GetAssetPaths()
		{
			return m_AssetPaths;
		}

		static std::string GetAssetTypeName(AssetType type)
		{
			switch (type)
			{
				case AssetType::MESH:		return "Mesh";
				case AssetType::TEXTURE_2D: return "Texture2D";
				case AssetType::SHADER:		return "Shader";
			}

			CR_ASSERT(false, "Unknown Type");
			return "invalid";
		}

	private:
		template<typename T>
		static Ref<Asset> LoadAsset(const std::string& filepath)
		{
			static_assert(false, "Could not find LoadAsset implementation");
		}

		template<>
		static Ref<Asset> LoadAsset<Mesh>(const std::string& filepath)
		{
			Ref<Asset> asset = CreateRef<Mesh>(filepath);
			asset->m_AssetType = AssetType::MESH;
			return asset;
		}

		template<>
		static Ref<Asset> LoadAsset<Texture2D>(const std::string& filepath)
		{
			Ref<Asset> asset = CreateRef<Texture2D>(filepath);
			asset->m_AssetType = AssetType::TEXTURE_2D;
			return asset;
		}

		template<>
		static Ref<Asset> LoadAsset<Shader>(const std::string& filepath)
		{
			Ref<Asset> asset = CreateRef<Shader>(filepath);
			asset->m_AssetType = AssetType::SHADER;
			return asset;
		}

		static std::string GetAbsolutePath(const std::string& path)
		{
			std::filesystem::path absolutePath = std::filesystem::canonical(path);
			if (std::filesystem::exists(absolutePath))
			{
				return absolutePath.string();
			}

			return std::string();
		}

	};

}