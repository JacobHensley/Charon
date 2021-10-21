#pragma once
#include "Charon/Core/Core.h"
#include "Charon/Asset/UUID.h"

namespace Charon {

	enum class AssetType
	{
		INVAILD = 0, MESH, TEXTURE_2D, SHADER
	};

    class Asset
    {
    public:
		virtual ~Asset() = default;

		AssetType m_AssetType;
	};

	struct AssetHandle
	{
		AssetHandle(UUID uuid)
			: m_UUID(uuid) 
		{
		}

		AssetHandle()
			: m_UUID({ 0 })
		{
		}

		UUID GetUUID() const { return m_UUID; }
		bool IsValid() const { return m_UUID != 0; }

		operator UUID () const { return m_UUID; }
		operator bool() const { return IsValid(); }

	private:
		UUID m_UUID;
	};

}