#define NOMINMAX
#include <Windows.h>
#include "Engine/ErrorBox.hpp"

void showErrorBox(const std::string &message, const std::string &title)
{
    MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_ICONERROR | MB_OK);
}