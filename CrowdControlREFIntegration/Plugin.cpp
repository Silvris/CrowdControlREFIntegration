#include <stdio.h>
#include <string.h>  
#include <stdlib.h>
#include <reframework/API.hpp>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <unordered_map>
#include <sol/sol.hpp>
#include <fstream>

#pragma comment(lib, "Ws2_32.lib")

//C++20 required

using namespace reframework;

typedef void* (*REFGenericFunction)(...);

lua_State* g_lua{ nullptr };

std::unordered_map<std::string, sol::load_result> g_loaded_snippets{};

SOCKET s; //Socket handle
WSADATA w;
char nullTerm[1] = {0};

bool IsConnected = false;
bool IsClosing = false;

void RunRequest(std::string str){
    API::LuaLock _{};

    if (g_lua == nullptr) {
        API::get()->log_error("%s","CC:g_lua nullptr");
        return;
    }

    sol::state_view lua{g_lua};
    lua["CCRequestString"] = str;
    std::string response = lua["CCRunRequest"]();
    API::get()->log_info("%s",response.c_str());
    send(s,response.c_str(),response.length(),NULL);
    send(s,nullTerm,sizeof(nullTerm),NULL);
    return;
}

void WaitForMessages(){
    int currentLength = 0;
    int bytesRead = 0;
    char buffer[1024];

    while(!IsClosing){
        currentLength = 0;
        do{
            if(currentLength >= sizeof(buffer)){
                API::get()->log_error("%s", "CC: Current message exceeds buffer length.");
                return;
            }

            bytesRead = recv(s,buffer + currentLength,1,NULL);

            if (bytesRead > 0){
                if(buffer[currentLength] == NULL) break;
            }

            currentLength += bytesRead;
        } while(bytesRead > 0);

        if(currentLength == -1){
            IsConnected = false;
        }
        RunRequest(std::string(buffer));
        
    }
}

//CONNECTTOHOST  Connects to a remote host
int ListenOnPort(int portno)
{
    int error = WSAStartup (0x0202, &w);  // Fill in WSA info

    if (error)
    {
        return -1;                     //For some reason we couldn't start Winsock
    }

    if (w.wVersion != 0x0202)             //Wrong Winsock version?
    {
        WSACleanup ();
        return -1;
    }

    SOCKADDR_IN addr;                     // The address structure for a TCP socket

    addr.sin_family = AF_INET;            // Address family
    addr.sin_port = htons (portno);       // Assign port to this socket

    //Accept a connection from any IP using INADDR_ANY
    //You could pass inet_addr("0.0.0.0") instead to accomplish the 
    //same thing. If you want only to watch for a connection from a 
    //specific IP, specify that            //instead.
    if (inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) <= 0) {
        API::get()->log_info("%s","CC: inet_pton failed");
		return -1;
	}
    while(!IsClosing){
			if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				API::get()->log_info("%s","CC: Unable to open socket");
				return -1;
			}

			//Connect
			if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
				API::get()->log_info("%s", "CC: Unable to connect");
				return -1;
			}
			else
				IsConnected = true;

        WaitForMessages();
    }
    return 0;
}

//CLOSECONNECTION � shuts down the socket and closes any connection on it
void CloseConnection()
{
    //Close the socket if it exists
    if (s)
        closesocket(s);

    WSACleanup();                     //Clean up Winsock
}

void on_lua_state_created(lua_State* l) {
    reframework::API::LuaLock _{};

    g_lua = l;
    g_loaded_snippets.clear();

    sol::state_view lua{ g_lua };

    // adds a new function to call from lua!
    lua["CCIsConnected"] = []() {
        return IsConnected;
    };

}

void on_lua_state_destroyed(lua_State* l) {
    API::LuaLock _{};

    g_lua = nullptr;
    g_loaded_snippets.clear();
}

static DWORD WINAPI ConnectCrowdControl(LPVOID lpParam) {
    ListenOnPort(64772);
    return 0;
}

extern "C" __declspec(dllexport) void reframework_plugin_required_version(REFrameworkPluginVersion* version) {
    version->major = REFRAMEWORK_PLUGIN_VERSION_MAJOR;
    version->minor = REFRAMEWORK_PLUGIN_VERSION_MINOR;
    version->patch = REFRAMEWORK_PLUGIN_VERSION_PATCH;
}

extern "C" __declspec(dllexport) bool reframework_plugin_initialize(const REFrameworkPluginInitializeParam* param) {
    reframework::API::initialize(param);
    DWORD thread;
    CreateThread(NULL,0,ConnectCrowdControl,0,0,&thread);
    param->functions->on_lua_state_created(on_lua_state_created);
    param->functions->on_lua_state_destroyed(on_lua_state_destroyed);
    return true;
}

BOOL APIENTRY DllMain(HANDLE handle, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_DETACH) {

        IsClosing = true;
        CloseConnection();
    }

    return TRUE;
}
