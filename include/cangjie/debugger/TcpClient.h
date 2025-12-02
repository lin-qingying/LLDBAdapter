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

#ifndef CANGJIE_DEBUGGER_TCP_CLIENT_H
#define CANGJIE_DEBUGGER_TCP_CLIENT_H


#include <string>

#ifdef _WIN32
#include <winsock2.h>
typedef SOCKET SOCKET_TYPE;
#else
typedef int SOCKET_TYPE;
#endif

namespace lldbprotobuf {
    class Request;
    class Event;
    class Response;
}

namespace Cangjie {
namespace Debugger {

class TcpClient {
public:
    TcpClient();
    ~TcpClient();

    bool Connect(const std::string& host, int port);
    void Disconnect();
    [[nodiscard]] bool IsConnected() const;

    [[nodiscard]] bool SendProtoMessage(const lldbprotobuf::Response &response) const;
    [[nodiscard]] bool SendEventBroadcast(const lldbprotobuf::Event& event) const;
    bool ReceiveData(char* buffer, size_t buffer_size, size_t& bytes_received);
    [[nodiscard]] bool ReceiveProtoMessage(lldbprotobuf::Request& request) const;



private:
    SOCKET_TYPE socket_;
    bool connected_;

#ifdef _WIN32
    bool wsa_initialized_;
#endif



};

} // namespace Debugger
} // namespace Cangjie

#endif // CANGJIE_DEBUGGER_TCP_CLIENT_H
