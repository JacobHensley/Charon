#pragma once
#include <cstdint>

namespace Charon {

	class UUID
	{
	public:
		UUID();
		UUID(uint64_t UUID);

		inline uint64_t GetUUID() { return m_UUID; }

		operator uint64_t() { return m_UUID; }
		bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
		bool operator!=(const UUID& other) const { return m_UUID != other.m_UUID; }

	private:
		uint64_t m_UUID;
	};

	struct UUIDHash 
	{
		std::size_t operator()(UUID e) const { return static_cast<std::size_t>(e); }
	};

}