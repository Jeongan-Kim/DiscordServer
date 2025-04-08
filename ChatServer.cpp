#include "ChatServer.h"
#include <iostream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")


std::string Trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \r\n\t");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \r\n\t");
    return str.substr(first, last - first + 1);
}


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

ChatServer::ChatServer() 
{
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

ChatServer::~ChatServer() 
{
    Stop();
    WSACleanup();
}

bool ChatServer::Start(int port) 
{
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

    // UDP 브로드캐스트 리스너 실행(클라이언트가 LAN 내에서 서버를 자동으로 찾을 수 있도록 도와주는 역할)
    std::thread([=]() 
        {
        SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (udpSocket == INVALID_SOCKET) return;

        sockaddr_in recvAddr = {};
        recvAddr.sin_family = AF_INET;
        recvAddr.sin_addr.s_addr = INADDR_ANY;
        recvAddr.sin_port = htons(50505); // 50505번 포트에서 UDP 수신 소켓을 열고 서버를 찾는 클라이언트를 기다림

        if (bind(udpSocket, (sockaddr*)&recvAddr, sizeof(recvAddr)) == SOCKET_ERROR) 
        {
            closesocket(udpSocket);
            return;
        }

        char buffer[512];
        sockaddr_in senderAddr;
        int senderSize = sizeof(senderAddr);

        while (isRunning) 
        {
            int len = recvfrom(udpSocket, buffer, sizeof(buffer) - 1, 0, (sockaddr*)&senderAddr, &senderSize);
            if (len > 0) {
                buffer[len] = '\0';
                if (std::string(buffer) == "DISCOVER_SERVER")  //클라이언트로부터 메시지를 받으면
                {
                    std::string response = "SERVER_HERE"; // 서버가 열려있다고 알려줌
                    sendto(udpSocket, response.c_str(), response.size(), 0, (sockaddr*)&senderAddr, sizeof(senderAddr));
                }
            }
        }

        closesocket(udpSocket);
        }).detach();

    std::cout << "서버가 실행되었습니다. 포트: " << port << "\n";
    return true;
}

void ChatServer::Stop() {
    isShuttingDown = true;
    isRunning = false;

    closesocket(listenSocket);
    std::lock_guard<std::mutex> lock(clientMutex);
    for (SOCKET s : clients) closesocket(s);
    clients.clear();
    clientNames.clear();
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
    std::string nickname = Trim(std::string(buffer));

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        clientNames[clientSocket] = nickname;
    }

    std::string joinMessage = nickname + "님이 입장하셨습니다.";
    Broadcast(Trim(joinMessage), INVALID_SOCKET); // 모두에게 입장 메시지 전송
    BroadcastUserList();  // 리스트도 같이 전송

    // 메시지 수신 루프
    while ((bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes] = '\0';

        std::string trimmed = Trim(std::string(buffer));

        // __DISCONNECT__ 메시지는 브로드캐스트하지 않고 종료 처리
        if (trimmed == "__DISCONNECT__") {
            std::cout << "[서버] 클라이언트 " << nickname << "가 종료 요청함." << std::endl;
            break;
        }

        std::string time = GetFormattedCurrentTime();

        std::lock_guard<std::mutex> lock(clientMutex);
        for (SOCKET s : clients) 
        {
            std::string label = "[" + nickname + "]"; // 나도 닉네임으로 표시
            std::string fullMessage = "[" + time + "] " + label + " " + Trim(buffer);
            send(s, fullMessage.c_str(), fullMessage.size(), 0);
        }
    }

    // 나갈 때 처리

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
        clientNames.erase(clientSocket);
    }

    closesocket(clientSocket);

    std::string exitMessage = nickname + "님이 퇴장하셨습니다.";
    Broadcast(Trim(exitMessage), INVALID_SOCKET); // 모두에게 퇴장 메시지 전송

    BroadcastUserList();  // 나간 뒤에도 업데이트
}

void ChatServer::Broadcast(const std::string& message, SOCKET exclude)
{
    if (!isRunning) return;

    std::vector<SOCKET> socketsToSend;
    std::vector<SOCKET> invalidSockets;

    {   // 🔒 Lock을 짧게만 유지하면서 복사만 한다
        std::lock_guard<std::mutex> lock(clientMutex);
        if (clients.empty()) return;

        for (SOCKET s : clients) {
            if (s != exclude) {
                socketsToSend.push_back(s);
            }
        }
    }

    for (SOCKET s : socketsToSend) {
        int result = send(s, message.c_str(), message.size(), 0);
        if (result == SOCKET_ERROR) {
            std::cerr << "send() error: " << WSAGetLastError() << std::endl;
            invalidSockets.push_back(s);
        }
    }

    // 🔥 죽은 소켓을 안전하게 제거
    if (!invalidSockets.empty()) {
        std::lock_guard<std::mutex> lock(clientMutex);
        for (SOCKET s : invalidSockets) {
            closesocket(s);
            clients.erase(std::remove(clients.begin(), clients.end(), s), clients.end());
            clientNames.erase(s);
        }
    }
}

void ChatServer::BroadcastUserList()
{
    if (!isRunning || isShuttingDown) return;

    std::string list = "[USER_LIST]";
    std::vector<SOCKET> invalidSockets;

    {

        std::lock_guard<std::mutex> lock(clientMutex);
        if (clients.empty()) return;  // ✅ 클라이언트가 없으면 아무것도 안 함

        for (const auto& [sock, name] : clientNames) {
            if (name.empty()) {
                invalidSockets.push_back(sock);
            }
            else {
                list += name + ";";
            }
        }

        for (SOCKET s : invalidSockets) {
            clientNames.erase(s);
            clients.erase(std::remove(clients.begin(), clients.end(), s), clients.end());
            closesocket(s);
        }
    }

    if (!list.empty()) {
        Broadcast(list); // isRunning은 Broadcast에서도 확인하도록
    }
}