#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <variant>

#include <boost/url.hpp>

namespace util {

struct ProgramOptions {

  [[nodiscard]] static std::variant<ProgramOptions, std::string>
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

    boost::url saveSourceUrl{""};
    size_t saveImageLimit{200};

    [[nodiscard]] static auto ParseJson(const std::filesystem::path &json)
        -> std::unordered_map<std::string, FeedOptions>;
    [[nodiscard]] static auto ParseJson(std::string_view jsonSv)
        -> std::unordered_map<std::string, FeedOptions>;
  };

  std::unordered_map<std::string, FeedOptions> feeds;

  boost::url hassUrl{""};
  std::string hassToken;

  std::string webUiHost{"0.0.0.0"};
  int webUiPort{32834};

  std::filesystem::path saveDestination;

  [[nodiscard]] bool CanSetupHass(const FeedOptions &feedOpts) const;
  [[nodiscard]] bool CanSetupFileSave(const FeedOptions &feedOpts) const;
};

} // namespace util
