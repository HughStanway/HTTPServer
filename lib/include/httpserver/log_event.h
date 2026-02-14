#ifndef LOG_EVENT_H
#define LOG_EVENT_H

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace HTTPServer {

class LogEvent {
 public:
  explicit LogEvent(const std::string& event) {
    fields.emplace_back("event", std::move(event));
  }

  LogEvent& add(const std::string& key, const std::string& value) {
    fields.emplace_back(std::move(key), std::move(value));
    return *this;
  }

  LogEvent& add(const std::string& key, int value) {
    return add(std::move(key), std::to_string(value));
  }

  LogEvent& add(const std::string& key, size_t value) {
    return add(std::move(key), std::to_string(value));
  }

  LogEvent& add(const std::string& key, bool value) {
    return add(std::move(key),
               value ? std::string("true") : std::string("false"));
  }

  std::string str() const {
    std::ostringstream oss;
    for (size_t i = 0; i < fields.size(); ++i) {
      if (i) oss << ' ';
      oss << fields[i].first << '=' << fields[i].second;
    }
    return oss.str();
  }

 private:
  std::vector<std::pair<std::string, std::string>> fields;
};

}  // namespace HTTPServer

#endif