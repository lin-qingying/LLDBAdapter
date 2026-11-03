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
 *
 * 日志采用异步模型：调用线程仅做级别过滤并入队，格式化与 I/O（console + 文件）
 * 全部由后台写线程完成，避免持锁同步写阻塞调试事件循环。
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
#include <atomic>
#include <thread>
#include <condition_variable>
#include <deque>

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
 * @brief Simple asynchronous logger for debugging
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
     * @brief Shutdown logger: flush pending messages and stop the writer thread
     */
    static void Shutdown();

    /**
     * @brief Set minimum log level
     * @param level Minimum log level
     */
    static void SetMinLevel(LogLevel level);

    // std::string 重载（保留既有调用方式）
    static void Debug(const std::string& message);
    static void Info(const std::string& message);
    static void Warning(const std::string& message);
    static void Error(const std::string& message);
    static void Critical(const std::string& message);

    // const char* 重载：字面量日志不再经过临时 std::string 构造即被拒绝
    static void Debug(const char* message);
    static void Info(const char* message);
    static void Warning(const char* message);
    static void Error(const char* message);
    static void Critical(const char* message);

    /**
     * @brief Log a message with specific level
     * @param level Log level
     * @param message Message to log
     */
    static void Log(LogLevel level, const std::string& message);

    /**
     * @brief Check if a log level is enabled (lock-free, atomic)
     * @param level Log level to check
     * @return true if enabled
     */
    static bool IsEnabled(LogLevel level);

private:
    static void WriteLog(LogLevel level, std::string&& message);
    static void Enqueue(LogLevel level, std::string&& message);
    static void WriterLoop();
    static std::string GetLevelString(LogLevel level);
    static std::string GetTimestamp();
    static std::string FormatMessage(LogLevel level, const std::string& message);

private:
    static std::unique_ptr<std::ofstream> s_log_file;
    static std::atomic<LogLevel> s_min_level;
    static bool s_enable_timestamp;
    static std::mutex s_mutex;          // 保护文件与队列元操作（Initialize/Shutdown）
    static std::atomic<bool> s_initialized;

    // 异步写线程与消息队列（队列受 s_queue_mutex 保护）
    static std::thread s_writer_thread;
    static std::mutex s_queue_mutex;
    static std::condition_variable s_queue_cv;
    static std::deque<std::pair<LogLevel, std::string>> s_queue;
    static bool s_shutdown_requested;
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
