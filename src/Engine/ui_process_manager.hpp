#pragma once

#include <windows.h>
#include <string>

class UIProcessManager {
public:
    UIProcessManager();
    ~UIProcessManager();

    bool start();
    void stop();

private:
    HANDLE process_handle_;
};
