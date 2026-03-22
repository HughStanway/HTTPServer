#ifndef LOG_STREAM_H
#define LOG_STREAM_H

#include <httpserver_impl/monitoring/log_level.h>

#include <sstream>

namespace HTTPServer {

class Logger;

template <typename L>
class LogStream {
 public:
  LogStream(LogLevel level, bool isErrno = false)
      : d_level(level), d_isErrno(isErrno) {}

  ~LogStream() {
    if (d_isErrno) {
      L::instance().logErrno(d_oss.str(), d_level);
    } else {
      L::instance().log(d_oss.str(), d_level);
    }
  }

  template <typename T>
  LogStream& operator<<(const T& msg) {
    d_oss << msg;
    return *this;
  }

 private:
  LogLevel d_level;
  bool d_isErrno;
  std::ostringstream d_oss;
};

}  // namespace HTTPServer

#endif