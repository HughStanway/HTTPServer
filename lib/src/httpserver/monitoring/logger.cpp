#include <errno.h>
#include <httpserver/monitoring/logger.h>
#include <httpserver/utils/config.h>
#include <pthread.h>
#include <string.h>

#include <thread>

namespace HTTPServer {

Logger& Logger::instance() {
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

std::string Logger::buildLogLine(const std::string& message, LogLevel level) {
  return "[" + currentTime() + "] " + "[" + levelToString(level) + "] " + "[" +
         currentThreadName() + "] " + message;
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
  return "thread-" + std::to_string(std::hash<std::thread::id>{}(
                         std::this_thread::get_id()));
#endif

  if (name[0] == '\0') {
    return "main-thread";
  }
  return std::string(name);
}

std::string Logger::generateLogFileName() {
  auto now = std::chrono::system_clock::now();
  std::time_t t_now = std::chrono::system_clock::to_time_t(now);
  char buf[64];
  std::strftime(buf, sizeof(buf), "httpserver-%Y%m%d-%H%M%S.log",
                std::localtime(&t_now));
  return std::string(buf);
}

void Logger::initializeLogFileIfNeeded() {
  if (d_logFileInitialized) {
    return;
  }

  d_logFilePath = generateLogFileName();
  d_logFile.open(d_logFilePath, std::ios::out | std::ios::app);
  d_logFileInitialized = true;
}

void Logger::log(const std::string& message, LogLevel level) {
  std::lock_guard<std::mutex> lock(d_mtx);

  // Only log messages at or above current level
  if (static_cast<int>(level) < static_cast<int>(d_currentLogLevel)) {
    return;
  }

  const std::string logLine = buildLogLine(message, level);

  std::cout << logLine << std::endl;
  if (Config::get().kFileLoggingEnabled) {
    initializeLogFileIfNeeded();
    if (d_logFile.is_open()) {
      d_logFile << logLine << std::endl;
    }
  }
}

void Logger::logErrno(const std::string& message, LogLevel level) {
  const int savedErrno = errno;

  char buffer[256];
#if defined(__APPLE__) || defined(__MUSL__)
  // XSI-compliant strerror_r returns int
  if (strerror_r(savedErrno, buffer, sizeof(buffer)) != 0) {
    strncpy(buffer, "Unknown error", sizeof(buffer));
  }
  std::string err(buffer);

#else
  std::string err(strerror_r(savedErrno, buffer, sizeof(buffer)));
#endif

  std::string errorMsg = message + ": " + err;
  log(errorMsg, level);
}

}  // namespace HTTPServer
