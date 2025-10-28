// ipc_client.hpp - Communication avec le processus principal via Named Pipe
#pragma once

#include <string>
#include <windows.h>

class IPCClient {
public:
    explicit IPCClient(const std::string& pipe_name) 
        : pipe_name_("\\\\.\\pipe\\" + pipe_name), pipe_handle_(INVALID_HANDLE_VALUE) {
        connect();
    }

    ~IPCClient() {
        disconnect();
    }

    bool send(const std::string& message) {
        if (pipe_handle_ == INVALID_HANDLE_VALUE) {
            if (!connect()) return false;
        }

        DWORD written;
        return WriteFile(pipe_handle_, message.c_str(), 
                        static_cast<DWORD>(message.size()), 
                        &written, nullptr) != 0;
    }

    std::string receive() {
        if (pipe_handle_ == INVALID_HANDLE_VALUE) return "";

        char buffer[4096];
        DWORD read;
        if (ReadFile(pipe_handle_, buffer, sizeof(buffer) - 1, &read, nullptr)) {
            buffer[read] = '\0';
            return std::string(buffer);
        }
        return "";
    }

private:
    bool connect() {
        // Tentative de connexion au pipe
        pipe_handle_ = CreateFileA(
            pipe_name_.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (pipe_handle_ == INVALID_HANDLE_VALUE) {
            DWORD error = GetLastError();
            if (error == ERROR_PIPE_BUSY) {
                // Attendre que le pipe soit disponible
                if (!WaitNamedPipeA(pipe_name_.c_str(), 5000)) {
                    return false;
                }
                return connect(); // Retry
            }
            return false;
        }

        DWORD mode = PIPE_READMODE_MESSAGE;
        SetNamedPipeHandleState(pipe_handle_, &mode, nullptr, nullptr);
        return true;
    }

    void disconnect() {
        if (pipe_handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(pipe_handle_);
            pipe_handle_ = INVALID_HANDLE_VALUE;
        }
    }

    std::string pipe_name_;
    HANDLE pipe_handle_;
};
