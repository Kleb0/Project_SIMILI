#pragma once

#include "ipc_server.hpp"
#include <windows.h>
#include <string>
#include <iostream>
#include <thread>
#include <chrono>

class ThreeDScene;

class UIProcessManager {
public:
    UIProcessManager() : process_handle_(nullptr), ipc_server_("SIMILI_IPC"), scene_(nullptr) {}

    ~UIProcessManager() {
        stop();
    }

    bool start() {
        ipc_server_.start([this](const std::string& msg) {
            onMessageFromUI(msg);
        });

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        std::string exe_path = "SIMILI_UI.exe"; 

        if (!CreateProcessA(
            nullptr,
            const_cast<char*>(exe_path.c_str()),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi)) {
            std::cerr << "Failed to launch SIMILI_UI.exe: " << GetLastError() << "\n";
            return false;
        }

        process_handle_ = pi.hProcess;
        CloseHandle(pi.hThread);

        std::cout << "SIMILI_UI.exe launched successfully\n";
        return true;
    }

    void stop() 
    {
        if (!process_handle_) 
        {
            return;
        }

        std::cout << "[UIProcessManager] Stopping UI process..." << std::endl;

        TerminateProcess(process_handle_, 0);
        CloseHandle(process_handle_);
        process_handle_ = nullptr;
        
        ipc_server_.stop();
        
        std::cout << "[UIProcessManager] UI process stopped." << std::endl;
    }

    void sendToUI(const std::string& message) 
    {
        ipc_server_.sendToUI(message);
    }

    void setScene(ThreeDScene* scene) 
    {
        scene_ = scene;
    }

    void sendObjectsListToUI();

private:
    void onMessageFromUI(const std::string& message) {
        std::cout << "[IPC] Message from UI: " << message << "\n";

        if (message == "browser_created") 
        {
            std::cout << "UI browser created\n";
            // Envoyer la liste des objets dès que le navigateur est créé
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            sendObjectsListToUI();
        } 
        else if (message == "browser_closed") 
        {
            std::cout << "UI browser closed\n";
        } 
        else if (message == "request_objects_list") 
        {
            std::cout << "UI requested objects list\n";
            sendObjectsListToUI();
        }
    }

    HANDLE process_handle_;
    IPCServer ipc_server_;
    ThreeDScene* scene_;
};
