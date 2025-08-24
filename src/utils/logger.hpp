#pragma once

#include <string>
#include <fstream>

namespace utils {

    // 日志级别枚举
    enum class LogLevel {
        INFO,
        WARNING,
        ERROR,
        DEBUG
    };

    // 日志管理器类
    class Logger {
    private:
        static const char* LOG_FILE_PATH;  // SD卡根目录日志文件路径
        static bool initialized;           // 初始化标志
        
        // 获取当前时间字符串
        static std::string GetCurrentTime();
        
        // 获取日志级别字符串
        static std::string GetLogLevelString(LogLevel level);
        
    public:
        // 初始化日志系统
        static bool Initialize();
        
        // 写入日志
        static void WriteLog(LogLevel level, const std::string& message);
        
        // 便捷方法
        static void Info(const std::string& message);
        static void Warning(const std::string& message);
        static void Error(const std::string& message);
        static void Debug(const std::string& message);
        
        // 清空日志文件
        static void ClearLog();
    };

} // namespace utils