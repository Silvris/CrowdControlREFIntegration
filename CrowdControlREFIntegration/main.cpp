#include <stdio.h>
#include <string.h>  
#include <stdlib.h>
#include "reframework/API.hpp"
#include <winsock.h>
#include <unordered_map>
#include "sol/sol.hpp"

#pragma comment(lib, "Ws2_32.lib")

using namespace reframework;

typedef void* (*REFGenericFunction)(...);

lua_State* g_lua{ nullptr };

std::unordered_map<std::string, sol::load_result> g_loaded_snippets{};

SOCKET s; //Socket handle

bool IsConnected;

//CONNECTTOHOST – Connects to a remote host
bool ConnectToHost(int PortNo, const char* IPAddress)
{
    //Start up Winsock…
    WSADATA wsadata;

    int error = WSAStartup(0x0202, &wsadata);

    //Did something happen?
    if (error)
        return false;

    //Did we get the right Winsock version?
    if(wsadata.wVersion != 0x0202)
    {
        WSACleanup(); //Clean up Winsock
        return false;
    }

    //Fill out the information needed to initialize a socket…
    SOCKADDR_IN target;               //Socket address information

    target.sin_family = AF_INET;      // address family Internet
    target.sin_port = htons(PortNo); //Port to connect on
    target.sin_addr.s_addr = inet_addr(IPAddress); //Target IP

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); //Create socket
    if (s == INVALID_SOCKET)
    {
        return false; //Couldn't create the socket
    }

    //Try connecting...

    if (connect(s, (SOCKADDR*)&target, sizeof(target)) == SOCKET_ERROR)
    {
        return false;                 //Couldn't connect
    }
    else
        return true;                  //Success
}

//CLOSECONNECTION – shuts down the socket and closes any connection on it
void CloseConnection()
{
    //Close the socket if it exists
    if (s)
        closesocket(s);

    WSACleanup();                     //Clean up Winsock
}

std::string ReceiveEffect() {

}


void on_lua_state_created(lua_State* l) {
    reframework::API::LuaLock _{};

    g_lua = l;
    g_loaded_snippets.clear();

    sol::state_view lua{ g_lua };

    // adds a new function to call from lua!
    lua["CCReceiveEffect"] = []() {
        return ReceiveEffect();
    };

    lua["CCIsConnected"] = []() {
        return IsConnected;
    };
}

void on_lua_state_destroyed(lua_State* l) {
    API::LuaLock _{};

    g_lua = nullptr;
    g_loaded_snippets.clear();

    CloseConnection();
}

void ConnectCrowdControl() {
    std::string ipa = "0.0.0.0";
    IsConnected = ConnectToHost(64772, ipa.c_str());
}

extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* version) {
    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    reframework::API::initialize(param);
    ConnectCrowdControl();
    param->functions->unlock_lua
    return true;
}
