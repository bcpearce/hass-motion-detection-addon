#pragma once

#include <string>
#include <string_view>
#include <variant>

#include <boost/url.hpp>

namespace util {

struct ProgramOptions {
  ProgramOptions() noexcept = default;
  ProgramOptions(int argc, const char **argv);
  boost::url url;
  std::string token;
  std::string username;
  std::string password;

  boost::url hassUrl{""};
  std::string hassEntityId;
  std::string hassFriendlyName;
  std::string hassToken;

  std::string webUiHost{"0.0.0.0"};
  int webUiPort{32834};

  std::variant<int, double> detectionSize = 0.05;
  std::chrono::seconds detectionDebounce{30};

  [[nodiscard]] bool CanSetupHass() const;
};

} // namespace util
