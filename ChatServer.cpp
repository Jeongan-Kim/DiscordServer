#include "ChatServer.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

std::string GetFormattedCurrentTime()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm;
    localtime_s(&local_tm, &time_t_now);
    std::ostringstream oss;
    oss << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

ChatServer::ChatServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

ChatServer::~ChatServer() {
    Stop();
    WSACleanup();
}

bool ChatServer::Start(int port) {
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) return false;
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) return false;

    isRunning = true;
    std::thread(&ChatServer::AcceptClients, this).detach();

    std::cout << "서버가 실행되었습니다.\n";
    return true;
}

void ChatServer::Stop() {
    isRunning = false;
    closesocket(listenSocket);
    std::lock_guard<std::mutex> lock(clientMutex);
    for (SOCKET s : clients) closesocket(s);
    clients.clear();
}

void ChatServer::AcceptClients() {
    while (isRunning) {
        SOCKET client = accept(listenSocket, nullptr, nullptr);
        if (client != INVALID_SOCKET) {
            std::lock_guard<std::mutex> lock(clientMutex);
            clients.push_back(client);
            std::thread(&ChatServer::HandleClient, this, client).detach();
        }
    }
}

void ChatServer::HandleClient(SOCKET clientSocket) {
    char buffer[1024];
    int bytes;

    // 처음에 닉네임 먼저 받기
    bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) return;
    buffer[bytes] = '\0';
    std::string nickname(buffer);

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        clientNames[clientSocket] = nickname;
    }

    std::string joinMessage = nickname + "님이 입장하셨습니다.";
    Broadcast(joinMessage, clientSocket);

    // 채팅 메시지 수신 루프
    while ((bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';

        std::string fullMessage = "[" + GetFormattedCurrentTime() + "] [" + nickname + "] " + buffer;
        Broadcast(fullMessage, clientSocket);
    }

    // 나갈 때 처리
    closesocket(clientSocket);

    std::lock_guard<std::mutex> lock(clientMutex);
    clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
    clientNames.erase(clientSocket);
}

void ChatServer::Broadcast(const std::string& message, SOCKET exclude) {
    std::lock_guard<std::mutex> lock(clientMutex);
    for (SOCKET s : clients) {
        
         send(s, message.c_str(), message.size(), 0);
        
    }
}
