// ipc_server.hpp - Serveur IPC pour communiquer avec SIMILI_UI.exe
#pragma once

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <windows.h>

class IPCServer {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    explicit IPCServer(const std::string& pipe_name) 
        : pipe_name_("\\\\.\\pipe\\" + pipe_name), 
          running_(false) {}

    ~IPCServer() {
        stop();
    }

    bool start(MessageCallback callback) {
        if (running_) return false;

        callback_ = callback;
        running_ = true;
        server_thread_ = std::thread(&IPCServer::serverLoop, this);
        return true;
    }

    void stop() {
        running_ = false;
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    bool sendToUI(const std::string& message) {
        // Envoyer au client connecté
        if (client_handle_ == INVALID_HANDLE_VALUE) {
            std::cerr << "[IPC Server] No client connected, cannot send message\n";
            return false;
        }

        DWORD written;
        bool result = WriteFile(client_handle_, message.c_str(), 
                        static_cast<DWORD>(message.size()), 
                        &written, nullptr) != 0;
        
        if (result) {
            FlushFileBuffers(client_handle_); // Force l'envoi immédiat
            std::cout << "[IPC Server] Sent " << written << " bytes to UI\n";
        } else {
            std::cerr << "[IPC Server] Failed to send message: " << GetLastError() << "\n";
        }
        
        return result;
    }

private:
    void serverLoop() {
        while (running_) {

            HANDLE pipe = CreateNamedPipeA(
                pipe_name_.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                1,
                4096,
                4096,
                0,
                nullptr
            );

            if (pipe == INVALID_HANDLE_VALUE) 
            {
                Sleep(100);
                continue;
            }

            if (ConnectNamedPipe(pipe, nullptr) || GetLastError() == ERROR_PIPE_CONNECTED) 
            {
                client_handle_ = pipe;

                while (running_) 
                {
                    char buffer[4096];
                    DWORD read;
                    
                    if (ReadFile(pipe, buffer, sizeof(buffer) - 1, &read, nullptr)) {
                        buffer[read] = '\0';
                        if (callback_) {
                            callback_(std::string(buffer));
                        }
                    } 
                    else 
                    {
                        break; 
                    }
                }

                DisconnectNamedPipe(pipe);
                client_handle_ = INVALID_HANDLE_VALUE;
            }

            CloseHandle(pipe);
        }
    }

    std::string pipe_name_;
    std::atomic<bool> running_;
    std::thread server_thread_;
    MessageCallback callback_;
    HANDLE client_handle_ = INVALID_HANDLE_VALUE;
};
