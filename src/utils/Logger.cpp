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
 *
 * 异步实现要点：
 * - 调用线程只做 IsEnabled（atomic 读）+ 入队，格式化与 I/O 由写线程完成；
 * - 文件按批次 flush，而不是逐条 flush；
 * - s_min_level / s_initialized 使用 atomic，消除无锁读数据竞争；
 * - 时间戳使用 localtime_r / localtime_s，保证线程安全；
 * - 队列设上限，写线程卡死时丢弃最旧日志而不是无限占用内存。
 */

#include "cangjie/debugger/Logger.h"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <ctime>

#ifdef _WIN32
#include <ctime>
#else
#include <ctime>
#endif

namespace Cangjie {
namespace Debugger {

namespace {
    // 队列上限：超过后丢弃最旧消息，防止写线程被 I/O 卡住时内存无限增长
    constexpr size_t MAX_QUEUE_SIZE = 10000;
}

// Static member definitions
std::unique_ptr<std::ofstream> Logger::s_log_file;
std::atomic<LogLevel> Logger::s_min_level(LogLevel::INFO);
bool Logger::s_enable_timestamp = true;  // 仅在 Initialize（持锁）中写，写线程中读
std::mutex Logger::s_mutex;              // Initialize/Shutdown 与文件状态
std::atomic<bool> Logger::s_initialized(false);

std::thread Logger::s_writer_thread;
std::mutex Logger::s_queue_mutex;
std::condition_variable Logger::s_queue_cv;
std::deque<std::pair<LogLevel, std::string>> Logger::s_queue;
bool Logger::s_shutdown_requested = false; // 受 s_queue_mutex 保护

void Logger::Initialize(const std::string& log_file_path,
                       LogLevel min_level,
                       bool enable_timestamp) {
    std::lock_guard<std::mutex> lock(s_mutex);

    s_min_level.store(min_level, std::memory_order_relaxed);
    s_enable_timestamp = enable_timestamp;

    if (!log_file_path.empty()) {
        s_log_file = std::make_unique<std::ofstream>(log_file_path, std::ios::app);
        if (!s_log_file->is_open()) {
            std::cerr << "Failed to open log file: " << log_file_path << std::endl;
            s_log_file.reset();
        }
    }

    if (!s_writer_thread.joinable()) {
        s_shutdown_requested = false;
        s_initialized.store(true, std::memory_order_release);
        s_writer_thread = std::thread(&Logger::WriterLoop);
    }
}

void Logger::Shutdown() {
    {
        std::lock_guard<std::mutex> queue_lock(s_queue_mutex);
        s_shutdown_requested = true;
    }
    s_queue_cv.notify_all();

    if (s_writer_thread.joinable()) {
        s_writer_thread.join();
    }

    std::lock_guard<std::mutex> lock(s_mutex);
    if (s_log_file && s_log_file->is_open()) {
        s_log_file->flush();
        s_log_file->close();
    }
    s_log_file.reset();
    s_initialized.store(false, std::memory_order_release);
}

void Logger::SetMinLevel(LogLevel level) {
    s_min_level.store(level, std::memory_order_relaxed);
}

void Logger::Debug(const std::string& message) {
    if (IsEnabled(LogLevel::DEBUG)) {
        Enqueue(LogLevel::DEBUG, std::string(message));
    }
}

void Logger::Info(const std::string& message) {
    if (IsEnabled(LogLevel::INFO)) {
        Enqueue(LogLevel::INFO, std::string(message));
    }
}

void Logger::Warning(const std::string& message) {
    if (IsEnabled(LogLevel::WARNING)) {
        Enqueue(LogLevel::WARNING, std::string(message));
    }
}

void Logger::Error(const std::string& message) {
    if (IsEnabled(LogLevel::ERROR)) {
        Enqueue(LogLevel::ERROR, std::string(message));
    }
}

void Logger::Critical(const std::string& message) {
    if (IsEnabled(LogLevel::CRITICAL)) {
        Enqueue(LogLevel::CRITICAL, std::string(message));
    }
}

void Logger::Debug(const char* message) {
    if (IsEnabled(LogLevel::DEBUG)) {
        Enqueue(LogLevel::DEBUG, std::string(message));
    }
}

void Logger::Info(const char* message) {
    if (IsEnabled(LogLevel::INFO)) {
        Enqueue(LogLevel::INFO, std::string(message));
    }
}

void Logger::Warning(const char* message) {
    if (IsEnabled(LogLevel::WARNING)) {
        Enqueue(LogLevel::WARNING, std::string(message));
    }
}

void Logger::Error(const char* message) {
    if (IsEnabled(LogLevel::ERROR)) {
        Enqueue(LogLevel::ERROR, std::string(message));
    }
}

void Logger::Critical(const char* message) {
    if (IsEnabled(LogLevel::CRITICAL)) {
        Enqueue(LogLevel::CRITICAL, std::string(message));
    }
}

void Logger::Log(LogLevel level, const std::string& message) {
    if (IsEnabled(level)) {
        Enqueue(level, std::string(message));
    }
}

bool Logger::IsEnabled(LogLevel level) {
    if (!s_initialized.load(std::memory_order_acquire)) {
        return false;
    }
    return level >= s_min_level.load(std::memory_order_relaxed);
}

void Logger::Enqueue(LogLevel level, std::string&& message) {
    {
        std::lock_guard<std::mutex> lock(s_queue_mutex);
        if (s_shutdown_requested) {
            return;
        }
        if (s_queue.size() >= MAX_QUEUE_SIZE) {
            s_queue.pop_front(); // 丢弃最旧日志，保证调用方不被阻塞
        }
        s_queue.emplace_back(level, std::move(message));
    }
    s_queue_cv.notify_one();
}

void Logger::WriterLoop() {
    for (;;) {
        std::pair<LogLevel, std::string> item;

        {
            std::unique_lock<std::mutex> lock(s_queue_mutex);
            s_queue_cv.wait(lock, [] {
                return s_shutdown_requested || !s_queue.empty();
            });

            if (s_queue.empty()) {
                if (s_shutdown_requested) {
                    return;
                }
                continue;
            }

            // 批量取空队列，减少唤醒次数
            while (!s_queue.empty()) {
                item = std::move(s_queue.front());
                s_queue.pop_front();

                // 持有队列锁期间不释放 item 引用，改为立即写出
                const std::string formatted = FormatMessage(item.first, item.second);

                if (item.first >= LogLevel::WARNING) {
                    std::cerr << formatted << '\n';
                } else {
                    std::cout << formatted << '\n';
                }

                if (s_log_file && s_log_file->is_open()) {
                    *s_log_file << formatted << '\n';
                }
            }

            if (s_log_file && s_log_file->is_open()) {
                s_log_file->flush(); // 按批次 flush，而非逐条
            }
        }
        std::cout.flush();
        std::cerr.flush();
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

    std::tm tm_buf{};
#ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
#else
    localtime_r(&time_t, &tm_buf);
#endif

    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
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
