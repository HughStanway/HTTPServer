#include "httpserver/logger.h"

#include <errno.h>
#include <string.h>

namespace HTTPServer {

Logger &Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(d_mtx);
    d_currentLogLevel = level;
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARN:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

std::string Logger::currentTime() {
    auto now = std::chrono::system_clock::now();
    std::time_t t_now = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t_now));
    return std::string(buf);
}

std::string Logger::currentThreadName() {
    char name[64] = {0};

#if defined(__APPLE__)
    pthread_getname_np(pthread_self(), name, sizeof(name));
#elif defined(__linux__)
    pthread_getname_np(pthread_self(), name, sizeof(name));
#else
    // Fallback if unsupported
    return "thread-" + std::to_string(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
#endif

    if (name[0] == '\0') {
        return "main-thread";
    }
    return std::string(name);
}

void Logger::log(const std::string &message, LogLevel level) {
    std::lock_guard<std::mutex> lock(d_mtx);

    // Only log messages at or above current level
    if (static_cast<int>(level) < static_cast<int>(d_currentLogLevel)) {
        return;
    }

    std::cout << "[" << currentTime() << "] "
          << "[" << levelToString(level) << "] "
          << "[" << currentThreadName() << "] "
          << message << std::endl;
}

void Logger::logErrno(const std::string &message, LogLevel level) {
    std::lock_guard<std::mutex> lock(d_mtx);

    char buffer[256];
#if defined(__APPLE__) || defined(__MUSL__)
    // XSI-compliant strerror_r returns int
    if (strerror_r(errno, buffer, sizeof(buffer)) != 0) {
        strncpy(buffer, "Unknown error", sizeof(buffer));
    }
    std::string err(buffer);

#else
    std::string err(strerror_r(errno, buffer, sizeof(buffer)));
#endif

    std::string errorMsg = message + ": " + err;
    log(errorMsg, level);
}

} // namespace HTTPServer