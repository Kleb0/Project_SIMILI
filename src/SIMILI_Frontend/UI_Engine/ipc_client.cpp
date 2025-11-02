// ipc_client.cpp - Implémentation
#include "ipc_client.hpp"
#include <iostream>

IPCClient::IPCClient(const std::string& pipe_name) 
	: pipe_name_("\\\\.\\pipe\\" + pipe_name), pipe_handle_(INVALID_HANDLE_VALUE) 
{
	connect();
}

IPCClient::~IPCClient() 
{
	disconnect();
}

bool IPCClient::send(const std::string& message) 
{
	if (pipe_handle_ == INVALID_HANDLE_VALUE) {
		if (!connect()) return false;
	}

	DWORD written;
	return WriteFile(pipe_handle_, message.c_str(), 
					static_cast<DWORD>(message.size()), 
					&written, nullptr) != 0;
}

std::string IPCClient::receive() 
{
	if (pipe_handle_ == INVALID_HANDLE_VALUE) return "";

	DWORD bytes_available = 0;
	if (!PeekNamedPipe(pipe_handle_, nullptr, 0, nullptr, &bytes_available, nullptr)) 
	{
		return "";
	}

	if (bytes_available == 0) {
		return "";
	}

	char buffer[4096];
	DWORD read;

	if (ReadFile(pipe_handle_, buffer, sizeof(buffer) - 1, &read, nullptr)) 
	{
		buffer[read] = '\0';
		std::cout << "[IPCClient] Received: " << std::string(buffer) << std::endl;
		return std::string(buffer);
	}
	return "";
}

bool IPCClient::connect() 
{
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

			if (!WaitNamedPipeA(pipe_name_.c_str(), 5000)) 
			{
				return false;
			}
			return connect();
		}
		return false;
	}

	DWORD mode = PIPE_READMODE_MESSAGE;
	SetNamedPipeHandleState(pipe_handle_, &mode, nullptr, nullptr);
	return true;
}

void IPCClient::disconnect() 
{
	if (pipe_handle_ != INVALID_HANDLE_VALUE) 
	{
		CloseHandle(pipe_handle_);
		pipe_handle_ = INVALID_HANDLE_VALUE;
	}
}
