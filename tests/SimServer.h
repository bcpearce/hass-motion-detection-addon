#ifndef INCLUDE_SIMSERVER_H
#define INCLUDE_SIMSERVER_H

#include <mongoose.h>

#include <thread>

#include <boost/url.hpp>
#include <gtest/gtest.h>

class SimServer {
protected:
  struct Token {};

public:
  static void Start(int port);
  static void Stop();
  static const boost::url &GetBaseUrl();

  explicit SimServer(Token, int port);
  ~SimServer() noexcept = default;

protected:
  SimServer() = delete;
  SimServer(const SimServer &) = delete;
  SimServer &operator=(const SimServer &) = delete;
  SimServer(SimServer &&) = delete;
  SimServer &operator=(SimServer &&) = delete;
};

#endif