#include <iostream>
#include <string>
#include "ChatServer.h"
#include "ChatClient.h"

int main() {
    std::cout << "모드를 선택하세요 (1: 서버, 2: 클라이언트): ";
    int mode;
    std::cin >> mode;

    if (mode == 1) {
        ChatServer server;
        if (server.Start(9000)) {
            std::cout << "엔터키를 누르면 서버를 종료합니다...\n";
            std::cin.ignore();
            std::cin.get();
            server.Stop();
        }
        else {
            std::cerr << "서버 시작 실패\n";
        }
    }
    else if (mode == 2) {
        std::cin.ignore(); // 버퍼 비우기
        ChatClient client;
        if (client.Connect("127.0.0.1", 9000)) {
            std::string msg;
            while (std::getline(std::cin, msg)) {
                client.Send(msg);
            }
        }
        else {
            std::cerr << "서버 연결 실패\n";
        }
    }

    return 0;
}
