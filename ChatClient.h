#pragma once
#include <winsock2.h>
#include <thread>
#include <string>

class ChatClient {
public:
    ChatClient();
    ~ChatClient();

    bool Connect(const std::string& ip, int port);
    void Send(const std::string& message);
    void StartReceiving();

private:
    SOCKET sock;
    bool isConnected = false;

    std::string nickname;
};
