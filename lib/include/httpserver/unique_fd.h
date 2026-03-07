#ifndef UNIQUE_FD_H
#define UNIQUE_FD_H

#include <unistd.h>

#include <utility>

namespace HTTPServer {

class UniqueFd {
 public:
  UniqueFd() : fd_(-1) {}
  explicit UniqueFd(int fd) : fd_(fd) {}

  ~UniqueFd() { reset(); }

  UniqueFd(const UniqueFd&) = delete;
  UniqueFd& operator=(const UniqueFd&) = delete;

  UniqueFd(UniqueFd&& other) noexcept : fd_{std::exchange(other.fd_, -1)} {}

  UniqueFd& operator=(UniqueFd&& other) noexcept {
    if (this != &other) {
      reset(std::exchange(other.fd_, -1));
    }
    return *this;
  }

  int get() const { return fd_; }
  bool is_valid() const { return fd_ >= 0; }

  int release() { return std::exchange(fd_, -1); }

  void reset(int fd = -1) {
    if (fd_ >= 0) {
      close(fd_);
    }
    fd_ = fd;
  }

 private:
  int fd_;
};

}  // namespace HTTPServer

#endif
