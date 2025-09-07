#include "nompch.h"
#include "ImGuiLog.h"
#include <fstream>
#include <filesystem>

namespace NomBotCore {
    std::map<std::string, std::vector<ImGuiLogEntry>> ImGuiLogManager::logs;
    std::mutex ImGuiLogManager::logMutex;
	bool ImGuiLogManager::m_ScrollToBottom = false;

    void ImGuiLogManager::AddLog(const std::string& logName, const std::string& message, LogSeverity severity) {
        std::lock_guard<std::mutex> lock(logMutex);
        logs[logName].push_back(ImGuiLogEntry{ message, severity });
        m_ScrollToBottom = true;
    }

    std::vector<ImGuiLogEntry> ImGuiLogManager::GetLog(const std::string& logName) {
        std::lock_guard<std::mutex> lock(logMutex);
        return logs[logName];
    }

    std::vector<std::string> ImGuiLogManager::GetLogNames() {
        std::lock_guard<std::mutex> lock(logMutex);
        std::vector<std::string> names;
        for (const auto& pair : logs)
            names.push_back(pair.first);
        return names;
    }

	void ImGuiLogManager::DumpAllLogsToFile(const std::string& Directory)
	{
		std::lock_guard<std::mutex> lock(logMutex);
		std::filesystem::create_directories(Directory);
        for (const auto& pair : logs) {
            const std::string& logName = pair.first;
            const std::vector<ImGuiLogEntry>& entries = pair.second;
            std::string filePath = Directory + "/" + logName + ".log";
            std::ofstream outFile(filePath, std::ios::app);
            if (outFile.is_open()) {
                for (const auto& entry : entries) {
                    std::string severityStr;
                    switch (entry.severity) {
                    case LogSeverity::Info:    severityStr = "[INFO] "; break;
                    case LogSeverity::Warning: severityStr = "[WARNING] "; break;
                    case LogSeverity::Error:   severityStr = "[ERROR] "; break;
                    }
                    outFile << severityStr << entry.message << std::endl;
                }
                outFile.close();
            }
            else {
                AddLog("ImGuiLogManager", "Failed to open log file: " + filePath, LogSeverity::Error);
            }
		}
	}
}