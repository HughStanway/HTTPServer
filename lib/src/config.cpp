#include "httpserver/config.h"

#include <toml++/toml.h>

#include <fstream>
#include <stdexcept>
#include <string>

namespace {

template <typename T>
std::optional<T> get_toml_value(const toml::table* table,
                                const std::string& key);

template <>
std::optional<int> get_toml_value<int>(const toml::table* table,
                                       const std::string& key) {
  if (!table) return std::nullopt;
  if (auto v = table->get_as<int64_t>(key)) return static_cast<int>(v->get());
  return std::nullopt;
}

template <>
std::optional<size_t> get_toml_value<size_t>(const toml::table* table,
                                             const std::string& key) {
  if (!table) return std::nullopt;
  if (auto v = table->get_as<int64_t>(key))
    return static_cast<size_t>(v->get());
  return std::nullopt;
}

template <>
std::optional<double> get_toml_value<double>(const toml::table* table,
                                             const std::string& key) {
  if (!table) return std::nullopt;
  if (auto v = table->get_as<double>(key)) return v->get();
  return std::nullopt;
}

template <>
std::optional<std::string> get_toml_value<std::string>(const toml::table* table,
                                                       const std::string& key) {
  if (!table) return std::nullopt;
  if (auto v = table->get_as<std::string>(key)) return v->get();
  return std::nullopt;
}

HTTPServer::LogLevel string_to_log_level(const std::string& level) {
  if (level == "WARN") return HTTPServer::LogLevel::WARN;
  if (level == "ERROR") return HTTPServer::LogLevel::ERROR;
  return HTTPServer::LogLevel::INFO;
}

HTTPServer::ServerConfig parse_config_file(const std::string& path) {
  HTTPServer::ServerConfig cfg{};  // defaults

  toml::table tbl;
  try {
    tbl = toml::parse_file(path);
  } catch (const toml::parse_error& err) {
    throw std::runtime_error("Config parse error: " +
                             std::string(err.description()));
  }

  /**
   * Server Ports
   */
  if (auto cleanup = tbl["ports"].as_table()) {
    if (auto v = get_toml_value<int>(cleanup, "server_port"))
      cfg.kPort = HTTPServer::Port(v.value());

    if (auto v = get_toml_value<int>(cleanup, "http_redirection_port"))
      cfg.kRedirectionPort = HTTPServer::Port(v.value());
  }

  if (cfg.kPort.value() < 1 || cfg.kRedirectionPort.value() < 1 ||
      cfg.kPort.value() > 65535 || cfg.kRedirectionPort.value() > 65535)
    throw std::runtime_error("ports must be in the range [1, 65535]");

  /**
   * Periodic Idle IP Cleanup
   */
  if (auto cleanup = tbl["idle-ip-cleanup"].as_table()) {
    if (auto v = get_toml_value<int>(cleanup, "interval_minutes"))
      cfg.kCleanupInterval = std::chrono::minutes(v.value());

    if (auto v = get_toml_value<int>(cleanup, "idle_timeout_minutes"))
      cfg.kIdleTimeout = std::chrono::minutes(v.value());
  }

  if (cfg.kCleanupInterval.count() <= 0)
    throw std::runtime_error("idle-ip-cleanup::interval_minutes must be > 0");

  if (cfg.kIdleTimeout.count() < 0)
    throw std::runtime_error(
        "idle-ip-cleanup::idle_timeout_minutes must be >= 0");

  /**
   * Networking
   */
  if (auto net = tbl["network"].as_table()) {
    if (auto v = get_toml_value<int>(net, "client_timeout_seconds"))
      cfg.kClientTimeoutSec = v.value();

    if (auto v = get_toml_value<int>(net, "recv_buffer_size"))
      cfg.kRecvBufferSize = v.value();
  }

  if (cfg.kClientTimeoutSec <= 0)
    throw std::runtime_error("network::client_timeout_seconds must be >= 0");

  if (cfg.kRecvBufferSize <= 0)
    throw std::runtime_error("network::recv_buffer_size must be > 0");

  /**
   * Threading
   */
  if (auto th = tbl["threading"].as_table()) {
    if (auto v = get_toml_value<size_t>(th, "min_threads"))
      cfg.kMinThreads = v.value();

    if (auto v = get_toml_value<size_t>(th, "max_threads"))
      cfg.kMaxThreads = v.value();

    if (auto v = get_toml_value<int>(th, "max_queue_size"))
      cfg.kMaxQueueSize = v.value();
  }

  if (cfg.kMinThreads <= 0 || cfg.kMaxThreads <= 0 ||
      cfg.kMinThreads > cfg.kMaxThreads)
    throw std::runtime_error(
        "threading::min_threads or threading::max_threads not "
        "valid");

  if (cfg.kMaxQueueSize <= 0)
    throw std::runtime_error("threading::max_queue_size must be > 0");

  /**
   * Rate limiting
   */
  if (auto rl = tbl["rate-limits"].as_table()) {
    if (auto v = get_toml_value<int>(rl, "max_connections_per_ip"))
      cfg.kMaxConnectionsPerIp = v.value();

    if (auto v = get_toml_value<int>(rl, "max_keep_alive_requests"))
      cfg.kMaxKeepAliveRequests = v.value();

    if (auto v = get_toml_value<double>(rl, "max_tokens"))
      cfg.kMaxTokens = v.value();

    if (auto v = get_toml_value<double>(rl, "refill_rate"))
      cfg.kRefillRate = v.value();
  }

  if (cfg.kMaxConnectionsPerIp <= 0)
    throw std::runtime_error("rate-limits::max_connections_per_ip must be > 0");

  if (cfg.kMaxKeepAliveRequests <= 0)
    throw std::runtime_error(
        "rate-limits::max_keep_alive_requests must be > 0");

  if (cfg.kMaxTokens <= 0.0)
    throw std::runtime_error("rate-limits::max_tokens must be > 0.0");

  if (cfg.kRefillRate <= 0.0)
    throw std::runtime_error("rate-limits::refill_rate must be > 0.0");

  /**
   * HTTP Limits
   */
  if (auto http = tbl["http-config"].as_table()) {
    if (auto v = get_toml_value<int>(http, "max_header_bytes"))
      cfg.kMaxHeaderBytes = v.value();

    if (auto v = get_toml_value<int>(http, "max_body_bytes"))
      cfg.kMaxBodyBytes = v.value();

    if (auto arr = http->get("allowed_methods")->as_array()) {
      cfg.kAllowedMethods.clear();
      for (auto& v : *arr)
        if (auto s = v.value<std::string>()) cfg.kAllowedMethods.insert(*s);
    }

    if (auto arr = http->get("allowed_versions")->as_array()) {
      cfg.kAllowedVersions.clear();
      for (auto& v : *arr)
        if (auto s = v.value<std::string>()) cfg.kAllowedVersions.insert(*s);
    }
  }

  if (cfg.kMaxHeaderBytes <= 0)
    throw std::runtime_error("http-config::max_header_bytes must be > 0");

  if (cfg.kMaxBodyBytes <= 0)
    throw std::runtime_error("http-config::max_body_bytes must be > 0");

  /**
   * Logging Level
   */
  if (auto net = tbl["logging"].as_table()) {
    if (auto v = get_toml_value<std::string>(net, "log_level")) {
      HTTPServer::LogLevel level = string_to_log_level(v.value());
      HTTPServer::Logger::instance().setLevel(level);
      cfg.kLogLevel = level;
    }
  }

  return cfg;
}

}  // namespace

namespace HTTPServer {

ServerConfig Config::d_config{};

void Config::initDefault() { d_config = ServerConfig{}; }

void Config::initFromFile(const std::string& path) {
  d_config = parse_config_file(path);
}

const ServerConfig& Config::get() { return d_config; }

}  // namespace HTTPServer