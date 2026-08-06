#ifndef PIM_LOGGER_H
#define PIM_LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace pim {

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

class Logger {
public:
    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void set_level(LogLevel level) { min_level_ = level; }
    void set_log_file(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (file_.is_open()) file_.close();
        file_.open(path, std::ios::app);
    }

    void log(LogLevel level, const char* file, int line, const char* func, const std::string& msg) {
        if (level < min_level_) return;
        std::lock_guard<std::mutex> lock(mutex_);
        std::ostringstream ss;
        build_prefix(ss, level, file, line);
        ss << msg;
        std::cout << ss.str() << std::endl;
        if (file_.is_open()) {
            file_ << ss.str() << std::endl;
            file_.flush();
        }
    }

private:
    Logger() = default;

    void build_prefix(std::ostringstream& ss, LogLevel level, const char* file, int line) {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S") << " ";
        ss << "[" << level_str(level) << "] ";
        const char* fname = strrchr(file, '/');
        ss << (fname ? fname + 1 : file) << ":" << line << " ";
    }

    const char* level_str(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO ";
            case LogLevel::WARN:  return "WARN ";
            case LogLevel::ERROR: return "ERROR";
            default: return "????";
        }
    }

    LogLevel min_level_ = LogLevel::INFO;
    std::mutex mutex_;
    std::ofstream file_;
};

} // namespace pim

#define PIM_LOG_STREAM(level, msg) do { \
    std::ostringstream _pim_ss; \
    _pim_ss << msg; \
    pim::Logger::instance().log(level, __FILE__, __LINE__, __func__, _pim_ss.str()); \
} while(0)

#define PIM_DEBUG(msg) PIM_LOG_STREAM(pim::LogLevel::DEBUG, msg)
#define PIM_INFO(msg)  PIM_LOG_STREAM(pim::LogLevel::INFO,  msg)
#define PIM_WARN(msg)  PIM_LOG_STREAM(pim::LogLevel::WARN,  msg)
#define PIM_ERROR(msg) PIM_LOG_STREAM(pim::LogLevel::ERROR, msg)

#endif