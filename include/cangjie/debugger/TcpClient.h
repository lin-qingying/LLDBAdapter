// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#ifndef CANGJIE_DEBUGGER_TCP_CLIENT_H
#define CANGJIE_DEBUGGER_TCP_CLIENT_H


#include <string>

#ifdef _WIN32
#include <winsock2.h>

namespace lldbprotobuf {
    class Request;
    class Event;
    class Response;
}

typedef SOCKET SOCKET_TYPE;
#else
typedef int SOCKET_TYPE;
#endif

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
