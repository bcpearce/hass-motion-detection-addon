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

  struct FeedOptions {
    boost::url sourceUrl;
    std::string sourceToken;
    std::string sourceUsername;
    std::string sourcePassword;

    std::string hassEntityId;
    std::string hassFriendlyName;

    std::variant<int, double> detectionSize = 0.05;
    std::chrono::seconds detectionDebounce{30};

    std::filesystem::path saveDestination;
    boost::url saveSourceUrl{""};
    size_t saveImageLimit{200};
  };

  std::vector<FeedOptions> feeds;

  boost::url hassUrl{""};
  std::string hassToken;

  std::string webUiHost{"0.0.0.0"};
  int webUiPort{32834};

  [[nodiscard]] bool CanSetupHass(const FeedOptions &feedOpts) const;
  [[nodiscard]] bool CanSetupFileSave(const FeedOptions &feedOpts) const;
};

} // namespace util
