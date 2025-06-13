#ifndef PROGRAM_OPTIONS_H
#define PROGRAM_OPTIONS_H

#include <string>
#include <string_view>
#include <variant>

#include <boost/url.hpp>

namespace util {

struct ProgramOptions {
  ProgramOptions() noexcept = default;
  ProgramOptions(int argc, const char **argv, bool loadDotEnv = true);
  boost::url url;
  std::string token;
  std::string username;
  std::string password;
  boost::url hassUrl;
  std::string hassEntityId;
  std::string hassToken;

  std::variant<int, double> detectionSize = 0.05;
  std::chrono::seconds detectionDebounce{30};

  [[nodiscard]] bool CanSetupHass() const;
};

} // namespace util

#endif
