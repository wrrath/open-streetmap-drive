#pragma once
#include <iostream>
#include <string_view>

namespace osm_drive::core {

enum class LogLevel { Info, Warning, Error };

inline void log(LogLevel level, std::string_view message) {
    const char* label = "INFO";
    if (level == LogLevel::Warning) label = "WARN";
    if (level == LogLevel::Error) label = "ERROR";
    std::cerr << '[' << label << "] " << message << '\n';
}

} // namespace osm_drive::core
