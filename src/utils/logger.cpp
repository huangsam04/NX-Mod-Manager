#include "logger.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <cstdio>

namespace utils {

    // 静态成员变量定义
    const char* Logger::LOG_FILE_PATH = "sdmc:/SSM2.txt";
    bool Logger::initialized = false;

    // 初始化日志系统
    bool Logger::Initialize() {
        if (initialized) {
            return true;
        }
        
        // 清空并创建新的日志文件（覆盖模式）
        std::ofstream test_file(LOG_FILE_PATH, std::ios::trunc);
        if (!test_file.is_open()) {
            return false;
        }
        test_file.close();
        
        initialized = true;
        
        // 写入初始化日志
        WriteLog(LogLevel::INFO, "SSM2 日志系统初始化完成 - 新会话开始");
        
        return true;
    }

    // 获取当前时间字符串
    std::string Logger::GetCurrentTime() {
        time_t now = time(0);
        tm* timeinfo = localtime(&now);
        
        std::stringstream ss;
        ss << std::setfill('0')
           << std::setw(4) << (timeinfo->tm_year + 1900) << "-"
           << std::setw(2) << (timeinfo->tm_mon + 1) << "-"
           << std::setw(2) << timeinfo->tm_mday << " "
           << std::setw(2) << timeinfo->tm_hour << ":"
           << std::setw(2) << timeinfo->tm_min << ":"
           << std::setw(2) << timeinfo->tm_sec;
        
        return ss.str();
    }

    // 获取日志级别字符串
    std::string Logger::GetLogLevelString(LogLevel level) {
        switch (level) {
            case LogLevel::INFO:    return "[INFO]";
            case LogLevel::WARNING: return "[WARN]";
            case LogLevel::ERROR:   return "[ERROR]";
            case LogLevel::DEBUG:   return "[DEBUG]";
            default:                return "[UNKNOWN]";
        }
    }

    // 写入日志
    void Logger::WriteLog(LogLevel level, const std::string& message) {
        if (!initialized) {
            return;
        }
        
        std::ofstream log_file(LOG_FILE_PATH, std::ios::app);
        if (!log_file.is_open()) {
            return;
        }
        
        // 格式: [时间] [级别] 消息内容
        log_file << GetCurrentTime() << " " 
                 << GetLogLevelString(level) << " " 
                 << message << std::endl;
        
        log_file.close();
    }

    // 便捷方法实现
    void Logger::Info(const std::string& message) {
        WriteLog(LogLevel::INFO, message);
    }

    void Logger::Warning(const std::string& message) {
        WriteLog(LogLevel::WARNING, message);
    }

    void Logger::Error(const std::string& message) {
        WriteLog(LogLevel::ERROR, message);
    }

    void Logger::Debug(const std::string& message) {
        WriteLog(LogLevel::DEBUG, message);
    }

    // 清空日志文件
    void Logger::ClearLog() {
        if (!initialized) {
            return;
        }
        
        std::ofstream log_file(LOG_FILE_PATH, std::ios::trunc);
        if (log_file.is_open()) {
            log_file.close();
            WriteLog(LogLevel::INFO, "日志文件已清空");
        }
    }

} // namespace utils