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

#include "cangjie/debugger/Logger.h"
#include "cangjie/debugger/TcpClient.h"
#include "cangjie/debugger/DebuggerClient.h"
#include <iostream>
#include <string>
#include <vector>

#if WIN32
#include <io.h>
#endif
#include <stdio.h>

int main(int argc, char** argv) {

#if WIN32
    // 检查适配器自己的 stdout 是不是终端
    int result = _isatty(_fileno(stdout));
    LOG_INFO("Adapter stdout isatty: %d\n", result);  // 0 = 管道，1 = 终端
#endif


    Cangjie::Debugger::Logger::Initialize("cangjie_debugger.log", Cangjie::Debugger::LogLevel::INFO, true);
    LOG_INFO("CangJie LLDB Frontend starting...");

    std::cout << "CangJie LLDB Frontend v1.0.0" << std::endl;
    std::cout << std::endl;

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <port>" << std::endl;
        std::cerr << "Example: " << argv[0] << " 8080" << std::endl;
        LOG_ERROR("No port number provided");
        Cangjie::Debugger::Logger::Shutdown();
        return 1;
    }

    int port = 0;
    try {
        port = std::stoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Error: Port must be between 1 and 65535" << std::endl;
            LOG_ERROR("Invalid port number: " + std::string(argv[1]));
            Cangjie::Debugger::Logger::Shutdown();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: Invalid port number '" << argv[1] << "'" << std::endl;
        LOG_ERROR("Failed to parse port number: " + std::string(e.what()));
        Cangjie::Debugger::Logger::Shutdown();
        return 1;
    }

    std::cout << "Connecting to TCP server localhost:" << port << "..." << std::endl;

    Cangjie::Debugger::TcpClient tcp_client;
    if (!tcp_client.Connect("127.0.0.1", port)) {
        std::cerr << "Failed to connect to port " << port << std::endl;
        LOG_ERROR("Connection failed");
        Cangjie::Debugger::Logger::Shutdown();
        return 1;
    }

    std::cout << "Connection successful" << std::endl;

    // 创建 DebuggerClient，构造函数会自动初始化 LLDB 并发送 InitializedEvent
    std::cout << "Initializing LLDB..." << std::endl;
    Cangjie::Debugger::DebuggerClient debugger_client(tcp_client);
    std::cout << "LLDB initialized" << std::endl;

    try {
        std::cout << "Entering message loop..." << std::endl;
        debugger_client.RunMessageLoop();
        LOG_INFO("CangJie LLDB Frontend exiting normally...");
    } catch (const std::exception& e) {
        std::cerr << "Exception in debugger: " << e.what() << std::endl;
        LOG_ERROR("Debugger exception: " + std::string(e.what()));
        Cangjie::Debugger::Logger::Shutdown();
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception in debugger" << std::endl;
        LOG_ERROR("Unknown debugger exception");
        Cangjie::Debugger::Logger::Shutdown();
        return 1;
    }

    Cangjie::Debugger::Logger::Shutdown();

    return 0;
}