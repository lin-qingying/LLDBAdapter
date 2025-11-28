// Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
// This source file is part of the Cangjie project, licensed under Apache-2.0
// with Runtime Library Exception.
//
// See https://cangjie-lang.cn/pages/LICENSE for license information.

#include "cangjie/debugger/TcpClient.h"
#include "cangjie/debugger/Logger.h"
#include <cstring>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <chrono>
#include <ctime>
#include <event.pb.h>
#include <request.pb.h>
#include <response.pb.h>
#include <vector>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif


namespace Cangjie::Debugger {

// ============================================
// 辅助函数：确保接收指定大小的数据
// ============================================
namespace {
    bool RecvExact(int socket, void* buffer, size_t size) {
        size_t total_received = 0;
        char* buf = static_cast<char*>(buffer);

        while (total_received < size) {
#ifdef _WIN32
            int received = recv(socket, buf + total_received, size - total_received, 0);
#else
            ssize_t received = recv(socket, buf + total_received, size - total_received, 0);
#endif
            if (received <= 0) {
                return false;
            }
            total_received += received;
        }
        return true;
    }

    bool SendExact(int socket, const void* buffer, size_t size) {
        size_t total_sent = 0;
        const char* buf = static_cast<const char*>(buffer);

        while (total_sent < size) {
#ifdef _WIN32
            int sent = send(socket, buf + total_sent, size - total_sent, 0);
#else
            ssize_t sent = send(socket, buf + total_sent, size - total_sent, 0);
#endif
            if (sent <= 0) {
                return false;
            }
            total_sent += sent;
        }
        return true;
    }
}

TcpClient::TcpClient()
    : socket_(0), connected_(false)
#ifdef _WIN32
    , wsa_initialized_(false)
#endif
{
}

TcpClient::~TcpClient() {
    Disconnect();
}

bool TcpClient::Connect(const std::string& host, int port) {
    if (connected_) {
        LOG_WARNING("Already connected");
        return true;
    }

    LOG_INFO("Attempting to connect to " + host + ":" + std::to_string(port));

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        LOG_ERROR("WSAStartup failed");
        return false;
    }
    wsa_initialized_ = true;
#endif

#ifdef _WIN32
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET) {
        int err = WSAGetLastError();
        LOG_ERROR("Failed to create socket, WSA error: " + std::to_string(err));
#else
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        LOG_ERROR("Failed to create socket, errno: " + std::to_string(errno));
#endif
#ifdef _WIN32
        WSACleanup();
        wsa_initialized_ = false;
#endif
        return false;
    }

    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

#ifdef _WIN32
    server_addr.sin_addr.s_addr = inet_addr(host.c_str());
#else
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);
#endif

    if (connect(socket_, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to connect to " + host + ":" + std::to_string(port));
#ifdef _WIN32
        closesocket(socket_);
        WSACleanup();
        wsa_initialized_ = false;
#else
        close(socket_);
#endif
        return false;
    }

    connected_ = true;
    LOG_INFO("Successfully connected to " + host + ":" + std::to_string(port));
    return true;
}

void TcpClient::Disconnect() {
    if (!connected_) {
        return;
    }

#ifdef _WIN32
    closesocket(socket_);
    if (wsa_initialized_) {
        WSACleanup();
        wsa_initialized_ = false;
    }
#else
    close(socket_);
#endif

    connected_ = false;
    LOG_INFO("Disconnected from server");
}

bool TcpClient::IsConnected() const {
    return connected_;
}

// ============================================
// 优化版本：一次发送（前4字节大小 + 消息内容）
// ============================================
bool TcpClient::SendProtoMessage(const lldbprotobuf::Response& response) const {
    if (!connected_) {
        LOG_ERROR("Not connected to server");
        return false;
    }

    // 序列化 protobuf 消息
    std::string serialized;
    if (!response.SerializeToString(&serialized)) {
        LOG_ERROR("Failed to serialize protobuf message");
        return false;
    }

    auto message_size = static_cast<uint32_t>(serialized.size());



    // 验证消息大小合理性
    constexpr uint32_t MAX_MESSAGE_SIZE = 100 * 1024 * 1024; // 100MB
    if (message_size == 0 || message_size > MAX_MESSAGE_SIZE) {
        LOG_ERROR("Invalid message size: " + std::to_string(message_size));
        return false;
    }

    // 构造完整数据包：[4字节大小(网络字节序)][消息内容]
    std::vector<char> packet(4 + message_size);
    uint32_t network_size = htonl(message_size);

    // 复制大小头部
    std::memcpy(packet.data(), &network_size, 4);
    // 复制消息内容
    std::memcpy(packet.data() + 4, serialized.data(), message_size);

    // 一次性发送完整数据包
    if (!SendExact(socket_, packet.data(), packet.size())) {
        LOG_ERROR("Failed to send protobuf message");
        return false;
    }

    LOG_INFO("Sent protobuf message of size: " + std::to_string(message_size) + " bytes");
    return true;
}

bool TcpClient::SendEventBroadcast(const lldbprotobuf::Event& event) const {
    if (!connected_) {
        LOG_ERROR("Not connected to server");
        return false;
    }



    // 将广播包装到 CompositeResponse 中
    lldbprotobuf::Response response;
    response.mutable_event()->CopyFrom(event);

    // 序列化 protobuf 消息
    std::string serialized;
    if (!response.SerializeToString(&serialized)) {
        LOG_ERROR("Failed to serialize broadcast message");
        return false;
    }

    auto message_size = static_cast<uint32_t>(serialized.size());

    // 验证消息大小合理性
    constexpr uint32_t MAX_MESSAGE_SIZE = 100 * 1024 * 1024; // 100MB
    if (message_size == 0 || message_size > MAX_MESSAGE_SIZE) {
        LOG_ERROR("Invalid broadcast size: " + std::to_string(message_size));
        return false;
    }

    // 构造完整数据包：[4字节大小(网络字节序)][消息内容]
    std::vector<char> packet(4 + message_size);
    uint32_t network_size = htonl(message_size);

    // 复制大小头部
    std::memcpy(packet.data(), &network_size, 4);
    // 复制消息内容
    std::memcpy(packet.data() + 4, serialized.data(), message_size);

    // 一次性发送完整数据包
    if (!SendExact(socket_, packet.data(), packet.size())) {
        LOG_ERROR("Failed to send broadcast message");
        return false;
    }

    LOG_INFO("Sent broadcast message of size: " + std::to_string(message_size) + " bytes");
    return true;
}

bool TcpClient::ReceiveData(char* buffer, size_t buffer_size, size_t& bytes_received) {
    if (!connected_) {
        LOG_ERROR("Not connected to server");
        return false;
    }

#ifdef _WIN32
    int received = recv(socket_, buffer, buffer_size, 0);
#else
    ssize_t received = recv(socket_, buffer, buffer_size, 0);
#endif

    if (received <= 0) {
        if (received == 0) {
            LOG_INFO("Connection closed by server");
        } else {
            LOG_ERROR("Failed to receive data");
        }
        connected_ = false;
        return false;
    }

    bytes_received = static_cast<size_t>(received);
    return true;
}

// ============================================
// 优化版本：使用 RecvExact 确保完整接收
// ============================================
bool TcpClient::ReceiveProtoMessage(lldbprotobuf::Request& request) const {
    if (!connected_) {
        LOG_ERROR("Not connected to server");
        return false;
    }

    // 接收 4 字节消息大小（使用 RecvExact 确保完整接收）
    uint32_t network_size;
    if (!RecvExact(socket_, &network_size, 4)) {
        LOG_ERROR("Failed to receive message size");
        return false;
    }

    // 转换为主机字节序
    uint32_t message_size = ntohl(network_size);

    // 验证消息大小合理性（防止内存攻击）
    constexpr uint32_t MAX_MESSAGE_SIZE = 100 * 1024 * 1024; // 100MB
    if (message_size > MAX_MESSAGE_SIZE) {
        LOG_ERROR("Invalid message size: " + std::to_string(message_size));
        return false;
    }

    // 处理空消息（size 0）- 跳过处理但不返回错误
    if (message_size == 0) {
        LOG_INFO("Received empty message (size 0), skipping");
        // 将request清空以确保这是一个有效的空消息
        request.Clear();
        return true;
    }

    // 分配接收缓冲区
    std::vector<char> message_data(message_size);

    // 接收完整消息内容（使用 RecvExact 确保完整接收）
    if (!RecvExact(socket_, message_data.data(), message_size)) {
        LOG_ERROR("Failed to receive message data");
        return false;
    }

    // 解析 protobuf 消息
    if (!request.ParseFromArray(message_data.data(), message_size)) {
        LOG_ERROR("Failed to parse protobuf message");
        return false;
    }


    LOG_INFO("Received protobuf message of size: " + std::to_string(message_size) + " bytes");
    return true;
}


} // namespace Cangjie::Debugger
