#pragma once
#include <winsock2.h>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <map>
#include <set>
#include <unordered_map>

class ChatServer {
public:
    ChatServer();
    ~ChatServer();

    bool Start(int port); // 실제 채팅 연결이 이루어지는 TCP 포트 번호를 매개로 받음
    void Stop();

private:
    void AcceptClients();
    void HandleClient(SOCKET clientSocket);
    void Broadcast(const std::string& message, SOCKET excludeSocket = INVALID_SOCKET); // 전체 공지사항 보낼 시
    void BroadcastUserList(const std::string& roomName); // 채팅방 이름을 인자로 받아 해당 방에만 리스트를 보내는 함수
    void BroadcastToRoom(const std::string& roomId, const std::string& message, SOCKET exclude); // 방 별로 채팅 메시지나 시스템 메시지 전송 시

    SOCKET listenSocket;
    std::vector<SOCKET> clients;
    std::mutex clientMutex;
    bool isRunning = false;
    bool isShuttingDown = false;

    //std::map<SOCKET, std::string> clientNames;
    //std::map<std::string, std::vector<SOCKET>> roomList;

    std::unordered_map<SOCKET, std::string> clientNames;            //소켓 -> 닉네임
    std::unordered_map<SOCKET, std::set<std::string>> clientRooms;  //소켓 -> 참가중인 방들   
    std::unordered_map<std::string, std::set<SOCKET>> roomList;     //방 이름 -> 참가중인 소켓들
};
