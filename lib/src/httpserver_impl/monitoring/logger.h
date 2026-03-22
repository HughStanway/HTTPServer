#ifndef LOGGER_H
#define LOGGER_H

#include <httpserver_impl/monitoring/log_level.h>
#include <httpserver_impl/monitoring/log_stream.h>

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
  bool shouldLogThisLevel(LogLevel level) const;

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

using LogStreamImpl = LogStream<Logger>;

#define LOG_STREAM(level)                                       \
  if (HTTPServer::Logger::instance().shouldLogThisLevel(level)) \
  HTTPServer::LogStreamImpl(level)

#define LOG_INFO LOG_STREAM(HTTPServer::LogLevel::INFO)
#define LOG_WARN LOG_STREAM(HTTPServer::LogLevel::WARN)
#define LOG_ERROR LOG_STREAM(HTTPServer::LogLevel::ERROR)
#define LOG_ERROR_ERRNO                                  \
  if (HTTPServer::Logger::instance().shouldLogThisLevel( \
          HTTPServer::LogLevel::ERROR))                  \
  HTTPServer::LogStreamImpl(HTTPServer::LogLevel::ERROR, true)

#define LOG_EVENT(level, event) \
  HTTPServer::Logger::instance().log((event).str(), level)

}  // namespace HTTPServer

#endif
