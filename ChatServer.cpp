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
    //oss << std::put_time(&local_tm, "%H:%M:%S");
    oss << std::put_time(&local_tm, "%H:%M");
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

        LoadUserDBFromFile("userDB.txt");

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

void ChatServer::HandleClient(SOCKET clientSocket)
{
    bool isLogined = false;
    std::string id, pw;

    char buffer[1024];
    int bytes;    


    while (!isLogined && (bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[bytes] = '\0';
        std::string firstMessage = Trim(std::string(buffer)); // LOGIN:<id>:<pw> 형식
        std::cout << "[서버] 수신 메시지: "  << firstMessage << std::endl;

        if (firstMessage.starts_with("REGISTER:"))
        {
            size_t firstColon = firstMessage.find(":");
            size_t secondColon = firstMessage.find(":", firstColon + 1);
            if (firstColon != std::string::npos && secondColon != std::string::npos)
            {
                std::string regID = Trim(firstMessage.substr(firstColon + 1, secondColon - firstColon - 1));
                std::string regPW = Trim(firstMessage.substr(secondColon + 1));

                std::cout << "regID : " << regID << std::endl;
                std::cout << "regPW : " << regPW << std::endl;

                // 아이디/비밀번호 비어있는 경우 체크!
                if (regID.empty() || regPW.empty())
                {
                    std::string response = "REGISTER_FAIL";
                    send(clientSocket, response.c_str(), response.size(), 0);
                    continue;
                }
                else 
                {
                    std::lock_guard<std::mutex> lock(userDataMutex);
                    if (userDB.count(regID) == 0)
                    {
                        userDB[regID] = regPW;

                        std::string response = "REGISTER_OK";
                        std::cout << "REGISTER_OK" << std::endl;

                        send(clientSocket, response.c_str(), response.size(), 0);

                        SaveUserDBToFile("userDB.txt"); // ← UserDB 저장!
                    }
                    else
                    {
                        std::string response = "REGISTER_FAIL";
                        send(clientSocket, response.c_str(), response.size(), 0);
                    }
                }                
            }
        }
        else if (firstMessage.starts_with("LOGIN:"))
        {
            size_t firstColon = firstMessage.find(":");
            size_t secondColon = firstMessage.find(":", firstColon + 1);

            LoginResult loginResult = LoginResult::LOGIN_FORMAT_ERROR; // 로그인 결과 메시지 기본 설정

            if (firstColon != std::string::npos && secondColon != std::string::npos)
            {
                std::cout << "로그인 정보 체크 시작" << std::endl;
                id = Trim(firstMessage.substr(firstColon + 1, secondColon - firstColon - 1));
                pw = Trim(firstMessage.substr(secondColon + 1));

                std::cout << "입력된 ID: [" << id << "]" << std::endl;
                std::cout << "입력된 PW: [" << pw << "]" << std::endl;


                auto it = userDB.find(id);

                if (it == userDB.end())
                {
                    loginResult = LoginResult::LOGIN_NO_ID;   // 등록된 아이디 없음
                    std::cout << "등록된 아이디 없음" << std::endl;

                }
                else if (it->second != pw)
                {
                    std::cout << "저장된 PW: [" << it->second << "]" << std::endl;

                    loginResult = LoginResult::LOGIN_WRONG_PW; // 아이디에 부합하는 PW 틀림
                    std::cout << "아이디에 부합하는 PW 틀림" << std::endl;


                }
                else if (clientSocket == INVALID_SOCKET)
                {
                    loginResult = LoginResult::LOGIN_CONNECT_ERROR; // 서버 연결 끊김
                }
                else
                {
                    std::lock_guard<std::mutex> lock(clientMutex);
                    if (std::find_if(clientNames.begin(), clientNames.end(), [&](const auto& pair) { return pair.second == id; }) != clientNames.end())
                    {
                        loginResult = LoginResult::LOGIN_ALREADY;       // 이미 로그인 중인 아이디
                        std::cout << "이미 로그인 중인 아이디" << std::endl;

                    }
                    else
                    {
                        clientNames[clientSocket] = id;
                        loginResult = LoginResult::LOGIN_SUCCESS;
                        std::cout << "[서버] 로그인 처리됨: " << id << std::endl;
                        isLogined = true;
                    }
                }
            }

            std::string response = ToString(loginResult);
            send(clientSocket, response.c_str(), response.size(), 0);

            if (!isLogined)
            {
                std::cout << "[서버] 로그인 실패, 재시도 가능\n";
                continue; // 루프 계속
            }

            break;
        }
    }

    if (!isLogined)
    {
        closesocket(clientSocket);
        return;
    }

    std::cout << "[서버] 로그인 완료: " << clientNames[clientSocket] << std::endl;

    // 메시지 수신 루프
    while ((bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) 
    {
        buffer[bytes] = '\0';

        std::string trimmed = Trim(std::string(buffer));
        
        std::cout << trimmed << std::endl;

        // __DISCONNECT__ 메시지는 브로드캐스트하지 않고 종료 처리
        if (trimmed == "__DISCONNECT__") {
            std::cout << "[서버] 클라이언트 " << id << "님이 종료 요청함." << std::endl;
            break;
        }

        // 채팅방 생성 처리
        if (trimmed.starts_with("CREATE_ROOM:"))
        {
            std::string roomName = trimmed.substr(strlen("CREATE_ROOM:"));
            std::lock_guard<std::mutex> lock(clientMutex);
            roomList[roomName];
            continue;
        }


        // 채팅방 참여 처리
        if (trimmed.starts_with("JOIN_ROOM:")) 
        {
            std::string roomName = trimmed.substr(std::string("JOIN_ROOM:").length());

            {
                std::lock_guard<std::mutex> lock(clientMutex);
                roomList[roomName].insert(clientSocket);
                clientRooms[clientSocket].insert(roomName);
            }

            std::string joinMessage = "SYSTEM:" + id + "님이 채팅방 [" + roomName + "]에 입장하셨습니다.";

            std::cout << joinMessage << std::endl;

            BroadcastToRoom(roomName, joinMessage, INVALID_SOCKET);
            BroadcastUserList(roomName); // 채팅방 참여했을 때 참여자 리스트 업데이트
            BroadcastVoiceListUpdate(roomName);
            continue;  // 다음 루프로 넘어감
        }

        // 채팅방 퇴장 처리
        if (trimmed.starts_with("LEAVE_ROOM:"))
        {
            std::string roomName = trimmed.substr(std::string("LEAVE_ROOM:").length());

            {
                std::lock_guard<std::mutex> lock(clientMutex);
                roomList[roomName].erase(clientSocket);
                clientRooms[clientSocket].erase(roomName);
            }

            std::string leaveMessage = "SYSTEM:" + id + "님이 채팅방 [" + roomName + "]에서 퇴장하셨습니다.";
            std::cout << leaveMessage << std::endl;

            BroadcastToRoom(roomName, leaveMessage, INVALID_SOCKET);
            BroadcastUserList(roomName);
            //BroadcastVoiceListUpdate(roomName);
            continue;
        }

        // 음성채널 참가 처리
        if (trimmed.starts_with("VOICE_JOIN:"))
        {
            std::string trim = trimmed.substr(std::string("VOICE_JOIN:").length());
            size_t p1 = trim.find(':');
            if (p1 == std::string::npos) return;

            std::string roomId = trim.substr(0, p1); //방 ID
            std::string sender = trim.substr(p1 + 1); // clientId

            voiceRooms[roomId].insert(clientSocket);
            BroadcastVoiceListUpdate(roomId, sender);
        }

        // 음성채널 퇴장 처리
        if (trimmed.starts_with("VOICE_LEAVE:"))
        {
            std::string trim = trimmed.substr(std::string("VOICE_LEAVE:").length());

            size_t p1 = trim.find(':');
            if (p1 == std::string::npos) return;

            std::string roomId = trim.substr(0, p1); //방 ID
            std::string sender = trim.substr(p1 + 1); // clientId

            BroadcastVoiceListUpdate(roomId, sender, false);
            //voiceRooms[roomId].erase(clientSocket);
        }

        // 일반 채팅 메시지 처리
        size_t p1 = trimmed.find(':'); // 첫 번째 ':' 위치
        size_t p2 = trimmed.find(':', p1 + 1); // 두 번째 ':' 위치

        if (p1 == std::string::npos || p2 == std::string::npos) return;

        std::string roomId = trimmed.substr(0, p1); // 방 ID
        std::string sender = trimmed.substr(p1 + 1, p2 - p1 - 1); // 보낸 사람
        std::string content = trimmed.substr(p2 + 1); // 메시지 내용

        std::string time = GetFormattedCurrentTime();
        //std::string fullMessage = "ROOMMSG:" + roomId + ":" + sender + ":" + content;
        std::string fullMessage = "ROOMMSG:" + roomId + ":" + time + ":" + sender + ":" + content;

        BroadcastToRoom(roomId, fullMessage, clientSocket);
    }

    // 나갈 때 처리(__DISCONNECT__로 프로그램이 종료되었을 때, 모든 채팅방에서 종료 처리)
    std::set<std::string> roomsToExit;
    {
        std::lock_guard<std::mutex> lock(clientMutex);
        roomsToExit = clientRooms[clientSocket];
        clientRooms.erase(clientSocket);

        for (const std::string& room : roomsToExit)
        {
            roomList[room].erase(clientSocket);
        }

        clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
        clientNames.erase(clientSocket);
    }

    for (const std::string& room : roomsToExit) 
    {
        std::string exitMessage = "SYSTEM:" + id + "님이 채팅방 [" + room + "]에서 퇴장하셨습니다.";
        BroadcastToRoom(room, exitMessage, INVALID_SOCKET);
        BroadcastUserList(room);
    }

    closesocket(clientSocket);
}

void ChatServer::EnsureDirectoryExists(const std::string& filepath)
{
    std::error_code ec;
    std::filesystem::path path(filepath);
    auto dir = path.parent_path();

    if (!dir.empty() && !std::filesystem::exists(dir, ec)) {
        if (!std::filesystem::create_directories(dir, ec)) {
            std::cerr << "[에러] 디렉토리 생성 실패: " << dir << "\n";
            std::cerr << "  사유: " << ec.message() << std::endl;
        }
        else {
            std::cout << "[서버] 디렉토리 생성 완료: " << dir << std::endl;
        }
    }
}

void ChatServer::SaveUserDBToFile(const std::string& filename)
{
    std::cout << "UserDB SAVE 시도" << "\n";

    EnsureDirectoryExists(filename); 

    std::ofstream ofs(filename);
    if (!ofs.is_open())
    {
        std::cerr << "[에러] 파일을 열 수 없습니다: " << filename << std::endl;
        perror("perror: ");
        return;
    }

    for (const auto& [id, pw] : userDB)
    {
        ofs << id << ":" << pw << "\n";
    }

    std::cout << "UserDB SAVE 완료" << "\n";

}

void ChatServer::LoadUserDBFromFile(const std::string& filename)
{
    std::lock_guard<std::mutex> lock(userDataMutex);
    std::ifstream ifs(filename);
    if (!ifs.is_open())
    {
        std::cerr << "파일을 열 수 없습니다: " << filename << std::endl;
        return;
    }

    std::string line;
    while (std::getline(ifs, line))
    {
        size_t sep = line.find(":");
        if (sep != std::string::npos)
        {
            std::string id = Trim(line.substr(0, sep));
            std::string pw = Trim(line.substr(sep + 1));
            if (!id.empty() && !pw.empty())
                userDB[id] = pw;
        }
    }

    std::cout << "UserDB LOAD 완료" << "\n";

}

void ChatServer::Broadcast(const std::string& message, SOCKET exclude)
{
    if (!isRunning) return;

    std::vector<SOCKET> socketsToSend;
    //std::vector<SOCKET> invalidSockets;

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        for (SOCKET s : clients)
            if (s != exclude) socketsToSend.push_back(s);
    }

    std::string toSend = message + "\n";

    for (SOCKET s : socketsToSend)
        send(s, toSend.c_str(), toSend.size(), 0);
}

void ChatServer::BroadcastUserList(const std::string& roomName)
{
    // USER_LIST:<roomName> : name1, name2, ...
    if (!isRunning || isShuttingDown) return;

    std::string list = "USER_LIST:" + roomName + ":";
    std::vector<SOCKET> targets;

    {
        std::lock_guard<std::mutex> lock(clientMutex);

        auto it = roomList.find(roomName);
        if (it == roomList.end()) return;

        for (SOCKET s : it->second)
        {
            if (clientNames.count(s))
            {
                list += clientNames[s] + ",";
                targets.push_back(s);
            }
        }
    }

    if (!list.empty() && list.back() == ',')
        list.pop_back();

    list += "\n";

    for (SOCKET s : targets)
    {
        send(s, list.c_str(), list.size(), 0);
    }
}

void ChatServer::BroadcastVoiceListUpdate(const std::string& roomName, const std::string& sender, bool isJoin)
{
    if (!isRunning || isShuttingDown) return;

    std::string list = "VOICE_LIST:" + roomName + ":";

    for (SOCKET sock : voiceRooms[roomName]) 
    {

        if (!isJoin && clientNames[sock] == sender)
        {
            voiceRooms[roomName].erase(sock);
            continue;// 나간 사용자 제외
        }
             

        list += clientNames[sock] + ",";
    }

    if (!voiceRooms[roomName].empty() && list.back() == ',')
        list.pop_back(); // 마지막 콤마 제거

    list += "\n";


    // 채팅방에 있는 모든 사람에게 브로드캐스트
    for (SOCKET sock : roomList[roomName])
    {
        send(sock, list.c_str(), list.length(), 0);
        std::cout << "[VOICE_LIST to " << clientNames[sock] << "] " << list << std::endl;
    }
}

void ChatServer::BroadcastToRoom(const std::string& roomId, const std::string& message, SOCKET exclude)
{
    if (!isRunning) return;

    std::vector<SOCKET> socketsToSend;

    {
        std::lock_guard<std::mutex> lock(clientMutex);
        auto it = roomList.find(roomId);
        if (it != roomList.end()) 
        {
            for (SOCKET s : it->second)
                socketsToSend.push_back(s);
        }
    }
    std::string toSend = message + "\n";

    for (SOCKET s : socketsToSend)
        send(s, toSend.c_str(), toSend.size(), 0);
}

