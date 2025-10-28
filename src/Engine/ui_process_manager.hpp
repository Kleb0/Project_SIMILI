// ui_process_manager.hpp - Gestionnaire du processus SIMILI_UI.exe
#pragma once

#include "ipc_server.hpp"
#include <windows.h>
#include <string>
#include <iostream>

class UIProcessManager {
public:
    UIProcessManager() : process_handle_(nullptr), ipc_server_("SIMILI_IPC") {}

    ~UIProcessManager() {
        stop();
    }

    bool start() {
        // Démarrer le serveur IPC
        ipc_server_.start([this](const std::string& msg) {
            onMessageFromUI(msg);
        });

        // Lancer SIMILI_UI.exe
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        std::string exe_path = "SIMILI_UI.exe"; // Doit être dans le même dossier

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

    void stop() {
        ipc_server_.stop();

        if (process_handle_) {
            // Envoyer signal de fermeture
            ipc_server_.sendToUI("quit");
            
            // Attendre 5 secondes
            if (WaitForSingleObject(process_handle_, 5000) == WAIT_TIMEOUT) {
                // Forcer la fermeture
                TerminateProcess(process_handle_, 0);
            }

            CloseHandle(process_handle_);
            process_handle_ = nullptr;
        }
    }

    void sendToUI(const std::string& message) {
        ipc_server_.sendToUI(message);
    }

private:
    void onMessageFromUI(const std::string& message) {
        std::cout << "[IPC] Message from UI: " << message << "\n";

        // Gérer les messages de l'UI
        if (message == "browser_created") {
            std::cout << "UI browser created\n";
        } else if (message == "browser_closed") {
            std::cout << "UI browser closed\n";
        }
        // Ajoutez vos propres handlers ici
    }

    HANDLE process_handle_;
    IPCServer ipc_server_;
};
