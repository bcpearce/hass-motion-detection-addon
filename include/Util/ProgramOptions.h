#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <variant>

#include <boost/url.hpp>

namespace util {

struct ProgramOptions {

  static std::variant<ProgramOptions, std::string>
  ParseOptions(int argc, const char **argv);

  boost::url sourceUrl;
  std::string sourceToken;
  std::string sourceUsername;
  std::string sourcePassword;

  boost::url hassUrl{""};
  std::string hassEntityId;
  std::string hassFriendlyName;
  std::string hassToken;

  std::string webUiHost{"0.0.0.0"};
  int webUiPort{32834};

  std::variant<int, double> detectionSize = 0.05;
  std::chrono::seconds detectionDebounce{30};
  std::filesystem::path saveDestination;
  boost::url saveSourceUrl{""};

  [[nodiscard]] bool CanSetupHass() const;
};

} // namespace util
