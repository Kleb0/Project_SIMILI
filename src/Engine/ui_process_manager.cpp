#include "ui_process_manager.hpp"
#include <iostream>

UIProcessManager::UIProcessManager() : process_handle_(nullptr) {}

UIProcessManager::~UIProcessManager() {
    stop();
}

bool UIProcessManager::start() {
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

void UIProcessManager::stop() {
    if (!process_handle_) {
        return;
    }

    std::cout << "[UIProcessManager] Stopping UI process..." << std::endl;

    TerminateProcess(process_handle_, 0);
    CloseHandle(process_handle_);
    process_handle_ = nullptr;
    
    std::cout << "[UIProcessManager] UI process stopped." << std::endl;
}
