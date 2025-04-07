#pragma once
#include <winsock2.h>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <map>

class ChatServer {
public:
    ChatServer();
    ~ChatServer();

    bool Start(int port);
    void Stop();

private:
    void AcceptClients();
    void HandleClient(SOCKET clientSocket);
    void Broadcast(const std::string& message, SOCKET excludeSocket);

    SOCKET listenSocket;
    std::vector<SOCKET> clients;
    std::mutex clientMutex;
    bool isRunning = false;

    std::map<SOCKET, std::string> clientNames;
};
