#include "DebugLogger.hpp"
#include <iostream>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>

namespace fs = std::filesystem;

DualLogger::DualStreamBuf::DualStreamBuf(std::streambuf* console, std::streambuf* file, std::ofstream* stream)
	: consoleBuf_(console)
	, fileBuf_(file)
	, fileStream_(stream)
{
}

int DualLogger::DualStreamBuf::overflow(int c)
{
	if (c != EOF)
	{
		consoleBuf_->sputc(c);
		fileBuf_->sputc(c);
		if (c == '\n' && fileStream_)
		{
			fileStream_->flush();
		}
	}
	return c;
}

std::streamsize DualLogger::DualStreamBuf::xsputn(const char* s, std::streamsize count)
{
	consoleBuf_->sputn(s, count);
	auto result = fileBuf_->sputn(s, count);
	if (fileStream_ && memchr(s, '\n', count))
	{
		fileStream_->flush();
	}
	return result;
}

DualLogger::DualLogger()
	: coutBackup_(nullptr)
	, cerrBackup_(nullptr)
	, coutDual_(nullptr)
	, cerrDual_(nullptr)
{
}

DualLogger::~DualLogger()
{
	if (coutBackup_ && cerrBackup_)
	{
		auto now = std::chrono::system_clock::now();
		auto time_t = std::chrono::system_clock::to_time_t(now);
		
		std::cout << "==========================================" << std::endl;
		std::cout << "[DualLogger] Session ended at: " << std::ctime(&time_t);
		std::cout << "==========================================" << std::endl;
		
		logFile_.flush(); // S'assurer que tout est écrit avant de fermer
		
		std::cout.rdbuf(coutBackup_);
		std::cerr.rdbuf(cerrBackup_);
	}
	
	if (coutDual_)
	{
		delete coutDual_;
	}
	
	if (cerrDual_)
	{
		delete cerrDual_;
	}
	
	if (logFile_.is_open())
	{
		logFile_.close();
	}
}

bool DualLogger::initialize(const std::string& logFilePath)
{
	// Vérifier si le fichier de log existe et le supprimer proprement
	if (fs::exists(logFilePath))
	{
		try
		{
			std::uintmax_t oldFileSize = fs::file_size(logFilePath);
			std::cout << "[DualLogger] Previous log file detected (" << oldFileSize << " bytes) - removing..." << std::endl;
			fs::remove(logFilePath);
			std::cout << "[DualLogger] Previous log file removed successfully" << std::endl;
		}
		catch (const fs::filesystem_error& e)
		{
			std::cerr << "[DualLogger] Warning: Could not remove previous log file: " << e.what() << std::endl;
		}
	}
	
	logFile_.open(logFilePath, std::ios::out | std::ios::trunc);
	if (!logFile_.is_open())
	{
		std::cerr << "[DualLogger] Failed to open log file: " << logFilePath << std::endl;
		return false;
	}
	
	std::cout << "[DualLogger] Log file created: " << logFilePath << std::endl;
	
	coutBackup_ = std::cout.rdbuf();
	cerrBackup_ = std::cerr.rdbuf();
	
	coutDual_ = new DualStreamBuf(coutBackup_, logFile_.rdbuf(), &logFile_);
	cerrDual_ = new DualStreamBuf(cerrBackup_, logFile_.rdbuf(), &logFile_);
	
	std::cout.rdbuf(coutDual_);
	std::cerr.rdbuf(cerrDual_);
	
	auto now = std::chrono::system_clock::now();
	auto time_t = std::chrono::system_clock::to_time_t(now);
	
	// Écrire l'en-tête de la nouvelle session 
	std::cout << "==========================================" << std::endl;
	std::cout << "[DualLogger] Session started at: " << std::ctime(&time_t);
	std::cout << "==========================================" << std::endl;
	
	logFile_.flush(); // S'assurer que l'en-tête est écrit immédiatement
	
	return true;
}
