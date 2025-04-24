#pragma once
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define NOMINMAX

#ifdef _WIN32
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstring>
// Windows 의 SOCKET 타입·상수 대체
typedef int SOCKET;
#define INVALID_SOCKET  (-1)
#define SOCKET_ERROR    (-1)
#define closesocket(s)  ::close(s)
#endif
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <fstream>
#include <filesystem>

enum class LoginResult {
    LOGIN_SUCCESS,
    LOGIN_NO_ID,
    LOGIN_WRONG_PW,
    LOGIN_FORMAT_ERROR,
    LOGIN_CONNECT_ERROR,
    LOGIN_ALREADY,
    LOGIN_ERROR
};

struct VoiceStatus
{
    bool micStatus = true;
    bool headsetStatus = true; 
    sockaddr_in udpEp;  // 클라이언트의 IP, UDP 포트
};

// 문자열로 변환 (서버가 클라이언트로 보낼 때 사용)
inline const char* ToString(LoginResult result)
{
    switch (result) {
    case LoginResult::LOGIN_SUCCESS:        return "LOGIN_SUCCESS";
    case LoginResult::LOGIN_NO_ID:          return "LOGIN_NO_ID";
    case LoginResult::LOGIN_WRONG_PW:       return "LOGIN_WRONG_PW";
    case LoginResult::LOGIN_FORMAT_ERROR:   return "LOGIN_FORMAT_ERROR";
    case LoginResult::LOGIN_CONNECT_ERROR:  return "LOGIN_CONNECT_ERROR";
    case LoginResult::LOGIN_ALREADY:        return "LOGIN_ALREADY";
    default:                                return "LOGIN_ERROR";
    }
}

class ChatServer {
public:
    ChatServer();
    ~ChatServer();

    bool Start(int port); // 실제 채팅 연결이 이루어지는 TCP 포트 번호를 매개로 받음
    void Stop();

private:
    void AcceptClients();
    void StartFileTransgerListrener();
    void HandleFileUpload(SOCKET clientSocket);
    void HandleClient(SOCKET clientSocket);

    void HandleClientAudio();

    void EnsureDirectoryExists(const std::string& filepath); // 상위 디렉토리가 존재하지 않으면 폴더를 만들어줌
    void SaveUserDBToFile(const std::string& filename);
    void LoadUserDBFromFile(const std::string& filename);

    void BroadcastRoomsInfoMSG();

    void Broadcast(const std::string& message, SOCKET excludeSocket = INVALID_SOCKET); // 전체 공지사항 보낼 시
    void BroadcastUserList(const std::string& roomName); // 채팅방 이름을 인자로 받아 해당 방에만 리스트를 보내는 함수
    void BroadcastVoiceListUpdate(const std::string& roomName, const std::string& sender = "", bool isJoin = true); // 방 전체에 채팅 참가자 ID 목록을 전송하는 함수
    void BroadcastToRoom(const std::string& roomId, const std::string& message, SOCKET exclude); // 방 별로 채팅 메시지나 시스템 메시지 전송 시
    void BroadcastFileToRoom(const std::string& roomId, const std::string& sender, const std::string& filename, std::vector<char> data);
    SOCKET listenSocket;
    std::vector<SOCKET> clients;

    std::mutex clientMutex;     // client 접근을 안전하게 하기 위해 사용하는 뮤텍스
    std::mutex userDataMutex;   // userDB 접근을 안전하게 하기 위해 사용하는 뮤텍스

    bool isRunning = false;
    bool isShuttingDown = false;

    std::unordered_map<std::string, std::string> userDB; //ID->Password 형태로 저장

    std::unordered_map<SOCKET, std::string> clientNames;            //소켓 -> 닉네임
    std::unordered_map<SOCKET, std::set<std::string>> clientRooms;  //소켓 -> 참가중인 방들   
    std::unordered_map<std::string, std::set<SOCKET>> roomList;     //방 이름 -> 참가중인 소켓들
    std::unordered_map<std::string, std::string> roomsInfo;           //방 이름 -> 방 비밀번호 

    std::unordered_map<std::string, std::set<SOCKET>> voiceRooms;           //roomId->해당 room의 voice채널에 참여중인 클라이언트
    //std::unordered_map<std::string, std::vector<sockaddr_in>> voiceEndpoints;  //roomId->UDP 엔드포인트(클라이언트 IP + UDP 포트)   -> 오디오 패키지를 보내려면 Socket 값만으로는 안되서 만든 자료구조
    std::unordered_map<std::string, std::unordered_map<std::string, VoiceStatus>> voiceEndpoints;  //roomId->(clientID -> UDP 엔드포인트(클라이언트 IP + UDP 포트) )  -> 오디오 패키지를 보내려면 Socket 값만으로는 안되서 만든 자료구조
};
