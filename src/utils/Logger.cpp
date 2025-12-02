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
 * This file implements the Logger class that provides logging functionality
 * for the Cangjie debugger.
 */

#include "cangjie/debugger/Logger.h"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace Cangjie {
namespace Debugger {

// Static member definitions
std::unique_ptr<std::ofstream> Logger::s_log_file;
LogLevel Logger::s_min_level = LogLevel::INFO;
bool Logger::s_enable_timestamp = true;
std::mutex Logger::s_mutex;
bool Logger::s_initialized = false;

void Logger::Initialize(const std::string& log_file_path,
                       LogLevel min_level,
                       bool enable_timestamp) {
    std::lock_guard<std::mutex> lock(s_mutex);

    s_min_level = min_level;
    s_enable_timestamp = enable_timestamp;
    s_initialized = true;

    if (!log_file_path.empty()) {
        s_log_file = std::make_unique<std::ofstream>(log_file_path, std::ios::app);
        if (!s_log_file->is_open()) {
            std::cerr << "Failed to open log file: " << log_file_path << std::endl;
            s_log_file.reset();
        }
    }
}

void Logger::Shutdown() {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (s_log_file && s_log_file->is_open()) {
        s_log_file->close();
    }
    s_log_file.reset();
    s_initialized = false;
}

void Logger::SetMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(s_mutex);
    s_min_level = level;
}

void Logger::Debug(const std::string& message) {
    if (IsEnabled(LogLevel::DEBUG)) {
        WriteLog(LogLevel::DEBUG, message);
    }
}

void Logger::Info(const std::string& message) {
    if (IsEnabled(LogLevel::INFO)) {
        WriteLog(LogLevel::INFO, message);
    }
}

void Logger::Warning(const std::string& message) {
    if (IsEnabled(LogLevel::WARNING)) {
        WriteLog(LogLevel::WARNING, message);
    }
}

void Logger::Error(const std::string& message) {
    if (IsEnabled(LogLevel::ERROR)) {
        WriteLog(LogLevel::ERROR, message);
    }
}

void Logger::Critical(const std::string& message) {
    if (IsEnabled(LogLevel::CRITICAL)) {
        WriteLog(LogLevel::CRITICAL, message);
    }
}

void Logger::Log(LogLevel level, const std::string& message) {
    if (IsEnabled(level)) {
        WriteLog(level, message);
    }
}

bool Logger::IsEnabled(LogLevel level) {
    if (!s_initialized) {
        return false;
    }
    return level >= s_min_level;
}

void Logger::WriteLog(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(s_mutex);

    if (!s_initialized) {
        return;
    }

    std::string formatted = FormatMessage(level, message);

    // Write to console
    if (level >= LogLevel::WARNING) {
        std::cerr << formatted << std::endl;
    } else {
        std::cout << formatted << std::endl;
    }

    // Write to file if available
    if (s_log_file && s_log_file->is_open()) {
        *s_log_file << formatted << std::endl;
        s_log_file->flush();
    }
}

std::string Logger::GetLevelString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:    return "DEBUG";
        case LogLevel::INFO:     return "INFO";
        case LogLevel::WARNING:  return "WARNING";
        case LogLevel::ERROR:    return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default:                 return "UNKNOWN";
    }
}

std::string Logger::GetTimestamp() {
    if (!s_enable_timestamp) {
        return "";
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

    return oss.str();
}

std::string Logger::FormatMessage(LogLevel level, const std::string& message) {
    std::ostringstream oss;

    if (s_enable_timestamp) {
        oss << "[" << GetTimestamp() << "] ";
    }

    oss << "[" << GetLevelString(level) << "] " << message;

    return oss.str();
}

} // namespace Debugger
} // namespace Cangjie