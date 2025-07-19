#pragma once

#include <mongoose.h>

#include <future>
#include <thread>

#include <boost/url.hpp>
#include <gtest/gtest.h>

namespace sim_token {
static constexpr const char *bearer{
    "5EA55652E6C64F0D8BDD72C16098248A"}; // pragma: allowlist secret
}

class SimServer {
protected:
  struct Token {};

public:
  static void Start(int port) noexcept;
  static void Stop();
  static const boost::url &GetBaseUrl();

  SimServer(Token, int port);
  ~SimServer() noexcept = default;

  static int GetHassApiCount();
  static int WaitForHassApiCount(int target, std::chrono::seconds timeout);
  static void ev_handler(struct mg_connection *c, int ev, void *ev_data);

protected:
  SimServer() = delete;
  SimServer(const SimServer &) = delete;
  SimServer &operator=(const SimServer &) = delete;
  SimServer(SimServer &&) = delete;
  SimServer &operator=(SimServer &&) = delete;
};