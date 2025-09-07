#ifndef __IMGUI_LOG_H__
#define __IMGUI_LOG_H__
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace NomBotCore {
    enum class LogSeverity {
        Info,
        Warning,
        Error
    };

    struct ImGuiLogEntry {
        std::string message;
        LogSeverity severity;
    };

    class ImGuiLogManager {
    public:
        static void AddLog(const std::string& logName, const std::string& message, LogSeverity severity = LogSeverity::Info);
        static std::vector<ImGuiLogEntry> GetLog(const std::string& logName);
        static std::vector<std::string> GetLogNames();
		static bool GetScrollToBottom() { return m_ScrollToBottom; }
		static void SetScrollToBottom(bool state) { m_ScrollToBottom = state; }
		static void DumpAllLogsToFile(const std::string& Directory);
    private:
        static std::map<std::string, std::vector<ImGuiLogEntry>> logs;
        static std::mutex logMutex;
		static bool m_ScrollToBottom;
    };
}
#endif