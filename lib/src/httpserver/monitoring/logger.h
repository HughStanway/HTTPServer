#ifndef LOGGER_H
#define LOGGER_H

#include <httpserver/monitoring/log_level.h>

#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>

namespace HTTPServer {

class Logger {
 public:
  static Logger& instance();
  void log(const std::string& message, LogLevel level = LogLevel::INFO);
  void logErrno(const std::string& message, LogLevel level = LogLevel::INFO);
  void setLevel(LogLevel level);

 private:
  Logger() = default;
  ~Logger() = default;
  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  std::string levelToString(LogLevel level);
  std::string buildLogLine(const std::string& message, LogLevel level);
  std::string currentTime();
  std::string currentThreadName();
  std::string generateLogFileName();
  void initializeLogFileIfNeeded();

  LogLevel d_currentLogLevel{LogLevel::INFO};
  std::mutex d_mtx;
  std::ofstream d_logFile;
  std::string d_logFilePath;
  bool d_logFileInitialized{false};
};

#define LOG_INFO(msg) \
  HTTPServer::Logger::instance().log(msg, HTTPServer::LogLevel::INFO)
#define LOG_WARN(msg) \
  HTTPServer::Logger::instance().log(msg, HTTPServer::LogLevel::WARN)
#define LOG_ERROR(msg) \
  HTTPServer::Logger::instance().log(msg, HTTPServer::LogLevel::ERROR)
#define LOG_ERROR_ERRNO(msg) \
  HTTPServer::Logger::instance().logErrno(msg, HTTPServer::LogLevel::ERROR)
#define LOG_EVENT(level, event) \
  HTTPServer::Logger::instance().log((event).str(), level)

}  // namespace HTTPServer

#endif
