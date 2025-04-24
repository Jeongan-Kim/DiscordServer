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
// Windows �� SOCKET Ÿ�ԡ���� ��ü
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
    sockaddr_in udpEp;  // Ŭ���̾�Ʈ�� IP, UDP ��Ʈ
};

// ���ڿ��� ��ȯ (������ Ŭ���̾�Ʈ�� ���� �� ���)
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

    bool Start(int port); // ���� ä�� ������ �̷������ TCP ��Ʈ ��ȣ�� �Ű��� ����
    void Stop();

private:
    void AcceptClients();
    void StartFileTransgerListrener();
    void HandleFileUpload(SOCKET clientSocket);
    void HandleClient(SOCKET clientSocket);

    void HandleClientAudio();

    void EnsureDirectoryExists(const std::string& filepath); // ���� ���丮�� �������� ������ ������ �������
    void SaveUserDBToFile(const std::string& filename);
    void LoadUserDBFromFile(const std::string& filename);

    void BroadcastRoomsInfoMSG();

    void Broadcast(const std::string& message, SOCKET excludeSocket = INVALID_SOCKET); // ��ü �������� ���� ��
    void BroadcastUserList(const std::string& roomName); // ä�ù� �̸��� ���ڷ� �޾� �ش� �濡�� ����Ʈ�� ������ �Լ�
    void BroadcastVoiceListUpdate(const std::string& roomName, const std::string& sender = "", bool isJoin = true); // �� ��ü�� ä�� ������ ID ����� �����ϴ� �Լ�
    void BroadcastToRoom(const std::string& roomId, const std::string& message, SOCKET exclude); // �� ���� ä�� �޽����� �ý��� �޽��� ���� ��
    void BroadcastFileToRoom(const std::string& roomId, const std::string& sender, const std::string& filename, std::vector<char> data);
    SOCKET listenSocket;
    std::vector<SOCKET> clients;

    std::mutex clientMutex;     // client ������ �����ϰ� �ϱ� ���� ����ϴ� ���ؽ�
    std::mutex userDataMutex;   // userDB ������ �����ϰ� �ϱ� ���� ����ϴ� ���ؽ�

    bool isRunning = false;
    bool isShuttingDown = false;

    std::unordered_map<std::string, std::string> userDB; //ID->Password ���·� ����

    std::unordered_map<SOCKET, std::string> clientNames;            //���� -> �г���
    std::unordered_map<SOCKET, std::set<std::string>> clientRooms;  //���� -> �������� ���   
    std::unordered_map<std::string, std::set<SOCKET>> roomList;     //�� �̸� -> �������� ���ϵ�
    std::unordered_map<std::string, std::string> roomsInfo;           //�� �̸� -> �� ��й�ȣ 

    std::unordered_map<std::string, std::set<SOCKET>> voiceRooms;           //roomId->�ش� room�� voiceä�ο� �������� Ŭ���̾�Ʈ
    //std::unordered_map<std::string, std::vector<sockaddr_in>> voiceEndpoints;  //roomId->UDP ��������Ʈ(Ŭ���̾�Ʈ IP + UDP ��Ʈ)   -> ����� ��Ű���� �������� Socket �������δ� �ȵǼ� ���� �ڷᱸ��
    std::unordered_map<std::string, std::unordered_map<std::string, VoiceStatus>> voiceEndpoints;  //roomId->(clientID -> UDP ��������Ʈ(Ŭ���̾�Ʈ IP + UDP ��Ʈ) )  -> ����� ��Ű���� �������� Socket �������δ� �ȵǼ� ���� �ڷᱸ��
};
