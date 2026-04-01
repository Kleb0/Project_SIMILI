#pragma once

#include <fstream>
#include <streambuf>
#include <string>
#include <filesystem>

class DualLogger
{
public:
	DualLogger();
	~DualLogger();

	bool initialize(const std::string& logFilePath);

private:
	class DualStreamBuf : public std::streambuf
	{
	private:
		std::streambuf* consoleBuf_;
		std::streambuf* fileBuf_;
		std::ofstream* fileStream_;

	public:
		DualStreamBuf(std::streambuf* console, std::streambuf* file, std::ofstream* stream);
		
		int overflow(int c) override;
		std::streamsize xsputn(const char* s, std::streamsize count) override;
	};

	std::ofstream logFile_;
	std::streambuf* coutBackup_;
	std::streambuf* cerrBackup_;
	DualStreamBuf* coutDual_;
	DualStreamBuf* cerrDual_;
};
