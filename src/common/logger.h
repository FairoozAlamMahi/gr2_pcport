#pragma once

#include <iostream>
#include <string_view>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace Log {

enum class Level {
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

inline void Print(Level level, std::string_view category, std::string_view msg) {
#ifndef GR2_ENABLE_DEBUG_LOGS
    if (level == Level::Debug) return;
#endif
    static std::mutex s_mutex;
    std::lock_guard<std::mutex> lock(s_mutex);

    const char* lvl_str = "INFO";
    const char* color_code = "\033[0m";

    switch (level) {
        case Level::Debug: lvl_str = "DEBUG"; color_code = "\033[36m"; break;
        case Level::Info:  lvl_str = "INFO "; color_code = "\033[32m"; break;
        case Level::Warn:  lvl_str = "WARN "; color_code = "\033[33m"; break;
        case Level::Error: lvl_str = "ERROR"; color_code = "\033[31m"; break;
        case Level::Fatal: lvl_str = "FATAL"; color_code = "\033[1;31m"; break;
    }

    static const auto s_start_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(now - s_start_time).count();

    char time_buf[16];
    std::snprintf(time_buf, sizeof(time_buf), "[%6.3fs]", sec);

    std::cout << color_code << time_buf << "[" << lvl_str << "][" << category << "] " 
              << msg << "\033[0m\n" << std::flush;
}

} // namespace Log

#define LOG_DEBUG(cat, msg) Log::Print(Log::Level::Debug, cat, msg)
#define LOG_INFO(cat, msg)  Log::Print(Log::Level::Info,  cat, msg)
#define LOG_WARN(cat, msg)  Log::Print(Log::Level::Warn,  cat, msg)
#define LOG_ERROR(cat, msg) Log::Print(Log::Level::Error, cat, msg)
#define LOG_FATAL(cat, msg) Log::Print(Log::Level::Fatal, cat, msg)
