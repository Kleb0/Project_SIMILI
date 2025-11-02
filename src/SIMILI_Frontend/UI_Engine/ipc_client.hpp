// ipc_client.hpp - Communication avec le processus principal via Named Pipe
#pragma once

#include <string>
#include <windows.h>

class IPCClient {
public:
    explicit IPCClient(const std::string& pipe_name);
    ~IPCClient();

    bool send(const std::string& message);
    std::string receive();

private:
    bool connect();
    void disconnect();

    std::string pipe_name_;
    HANDLE pipe_handle_;
};
