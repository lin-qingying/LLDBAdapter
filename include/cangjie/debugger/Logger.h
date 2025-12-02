/*
 * Copyright 2025 LinQingYing. and contributors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * The use of this source code is governed by the Apache License 2.0,
 * which allows users to freely use, modify, and distribute the code,
 * provided they adhere to the terms of the license.
 *
 * The software is provided "as-is", and the authors are not responsible for
 * any damages or issues arising from its use.
 *
 */

/**
 * @file
 *
 * This file declares the Logger class that provides logging functionality
 * for the Cangjie debugger.
 */

#ifndef CANGJIE_DEBUGGER_LOGGER_H
#define CANGJIE_DEBUGGER_LOGGER_H

// Undefine Windows macros that conflict with our enum
#ifdef _WIN32
#ifdef ERROR
#undef ERROR
#endif
#ifdef WARNING
#undef WARNING
#endif
#ifdef DEBUG
#undef DEBUG
#endif
#endif

#include <string>
#include <fstream>
#include <memory>
#include <mutex>

namespace Cangjie {
namespace Debugger {

/**
 * @brief Log level enumeration
 */
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    CRITICAL = 4
};

/**
 * @brief Simple logger class for debugging
 */
class Logger {
public:
    /**
     * @brief Initialize logger with configuration
     * @param log_file_path Optional log file path (empty for console only)
     * @param min_level Minimum log level to output
     * @param enable_timestamp Whether to include timestamps
     */
    static void Initialize(const std::string& log_file_path = "",
                          LogLevel min_level = LogLevel::INFO,
                          bool enable_timestamp = true);

    /**
     * @brief Shutdown logger
     */
    static void Shutdown();

    /**
     * @brief Set minimum log level
     * @param level Minimum log level
     */
    static void SetMinLevel(LogLevel level);

    /**
     * @brief Log a debug message
     * @param message Message to log
     */
    static void Debug(const std::string& message);

    /**
     * @brief Log an info message
     * @param message Message to log
     */
    static void Info(const std::string& message);

    /**
     * @brief Log a warning message
     * @param message Message to log
     */
    static void Warning(const std::string& message);

    /**
     * @brief Log an error message
     * @param message Message to log
     */
    static void Error(const std::string& message);

    /**
     * @brief Log a critical message
     * @param message Message to log
     */
    static void Critical(const std::string& message);

    /**
     * @brief Log a message with specific level
     * @param level Log level
     * @param message Message to log
     */
    static void Log(LogLevel level, const std::string& message);

    /**
     * @brief Check if a log level is enabled
     * @param level Log level to check
     * @return true if enabled
     */
    static bool IsEnabled(LogLevel level);

private:
    static void WriteLog(LogLevel level, const std::string& message);
    static std::string GetLevelString(LogLevel level);
    static std::string GetTimestamp();
    static std::string FormatMessage(LogLevel level, const std::string& message);

private:
    static std::unique_ptr<std::ofstream> s_log_file;
    static LogLevel s_min_level;
    static bool s_enable_timestamp;
    static std::mutex s_mutex;
    static bool s_initialized;
};

// Convenience macros for logging
#define LOG_DEBUG(msg) Cangjie::Debugger::Logger::Debug(msg)
#define LOG_INFO(msg) Cangjie::Debugger::Logger::Info(msg)
#define LOG_WARNING(msg) Cangjie::Debugger::Logger::Warning(msg)
#define LOG_ERROR(msg) Cangjie::Debugger::Logger::Error(msg)
#define LOG_CRITICAL(msg) Cangjie::Debugger::Logger::Critical(msg)

} // namespace Debugger
} // namespace Cangjie

#endif // CANGJIE_DEBUGGER_LOGGER_H