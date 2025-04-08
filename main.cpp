#include <iostream>
#include "ChatServer.h"

int main() {
    ChatServer server;

    if (server.Start(9000)) {
        std::cout << "서버가 실행 중입니다.\n";
        std::cin.get();  // 엔터를 눌러 서버 종료
        server.Stop();
    }
    else {
        std::cerr << "서버 시작 실패\n";
    }

    return 0;
}