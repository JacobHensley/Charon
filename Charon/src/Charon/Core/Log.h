#pragma once
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/fmt/ostr.h"

namespace Charon {

	class Log
	{
	public:
		static void Init();

	public:
		inline static std::shared_ptr<spdlog::logger>& GetLogger() { return s_Logger; }

	private:
		static std::shared_ptr<spdlog::logger> s_Logger;
	};

}

// Core log macros
#define CR_LOG_TRACE(...)    Charon::Log::GetLogger()->trace(__VA_ARGS__)
#define CR_LOG_DEBUG(...)    Charon::Log::GetLogger()->debug(__VA_ARGS__)
#define CR_LOG_INFO(...)     Charon::Log::GetLogger()->info(__VA_ARGS__)
#define CR_LOG_WARN(...)     Charon::Log::GetLogger()->warn(__VA_ARGS__)
#define CR_LOG_ERROR(...)    Charon::Log::GetLogger()->error(__VA_ARGS__)
#define CR_LOG_CRITICAL(...) Charon::Log::GetLogger()->critical(__VA_ARGS__)