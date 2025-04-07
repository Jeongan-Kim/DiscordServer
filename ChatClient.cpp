#include "ChatClient.h"
#include <iostream>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

ChatClient::ChatClient() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

ChatClient::~ChatClient() {
    closesocket(sock);
    WSACleanup();
}

bool ChatClient::Connect(const std::string& ip, int port) {
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return false;

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        std::cerr << "IP 주소 변환 실패\n";
        return false;
    }

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        return false;
    }

    isConnected = true;

	// 닉네임 입력
    std::cout << "닉네임을 입력하세요: ";
    std::getline(std::cin, nickname);

    // 서버에 닉네임 먼저 전송
    send(sock, nickname.c_str(), nickname.length(), 0);


    StartReceiving();
    return true;
}

void ChatClient::Send(const std::string& message) {
    if (isConnected) {
        send(sock, message.c_str(), message.size(), 0);
    }
}

void ChatClient::StartReceiving() {
    std::thread([this]() {
        char buffer[1024];
        int bytes;

        while ((bytes = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes] = '\0';

            // 현재 입력 줄 지우기 + 메시지 출력
            std::cout << "\r\33[K " << buffer << std::endl;

            // 입력 프롬프트 다시 출력
            std::cout << "> ";
            std::cout.flush();  // 출력 즉시 반영
        }
        }).detach();
}
