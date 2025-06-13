#include "WindowsWrapper.h"

#include "Util/ProgramOptions.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include <boost/program_options.hpp>

using namespace std::string_literals;
using namespace std::string_view_literals;
namespace po = boost::program_options;

namespace util {

ProgramOptions::ProgramOptions(int argc, const char **argv, bool loadDotEnv) {
  po::options_description desc("Allowed options");

  desc.add_options()("help", "show help message");
  desc.add_options()("version", "show version");
  desc.add_options()("source-url", po::value<std::string>()->required(),
                     "target video source URL");
  desc.add_options()("source-username",
                     po::value<std::string>()->default_value(""),
                     "target video source username for basic or digest auth");
  desc.add_options()("source-password",
                     po::value<std::string>()->default_value(""),
                     "target video source password for basic or digest auth");
  desc.add_options()("source-token",
                     po::value<std::string>()->default_value(""),
                     "target video source token authentication");
  desc.add_options()("hass-url", po::value<std::string>()->default_value(""),
                     "Home Assistant URL to send detector updates");
  desc.add_options()("hass-entity-id",
                     po::value<std::string>()->default_value(""),
                     "Home Assistant entity ID to update");
  desc.add_options()("hass-token", po::value<std::string>()->default_value(""),
                     "Home Assistant long-lived access token for API auth");
  desc.add_options()("detection-size", po::value<std::string>(),
                     "minimum size of a motion detection 'blob' that registers "
                     "as a region of interest\n"
                     "can be registered as an integer pixel count e.g. '500' "
                     "or a percentage of the total pixels e.g. '5%'");
  desc.add_options()(
      "detection-debounce", po::value<int>(),
      "minimum time between state changes in detection events (i.e. going from "
      "no motion to motion detected) in seconds");

  po::positional_options_description posOpts;
  posOpts.add("source-url", -1);

  po::variables_map vm;

  po::store(po::command_line_parser(argc, argv)
                .options(desc)
                .positional(posOpts)
                .run(),
            vm);

  const auto parsedEnv =
      po::parse_environment(desc, [](std::string_view envVar) {
        if (envVar == "USERNAME"sv) {
          return "source-username"s;
        }
        if (envVar == "PASSWORD"sv) {
          return "source-password"s;
        }
        if (envVar == "URL"sv) {
          return "source-url"s;
        }
        if (envVar == "HASS_URL"sv) {
          return "hass-url"s;
        }
        if (envVar == "HASS_ENTITY_ID"sv) {
          return "hass-entity-id"s;
        }
        if (envVar == "HASS_TOKEN"sv) {
          return "hass-token"s;
        }
        if (envVar == "DETECTION_SIZE"sv) {
          return "detection-size"s;
        }
        if (envVar == "DETECTION_DEBOUNCE"sv) {
          return "detection-debounce"s;
        }
        return ""s;
      });
  po::store(parsedEnv, vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    exit(0);
  }

  if (vm.count("version")) {
    std::cout << MOTION_DETECTION_SEMVER << "\n";
    exit(0);
  }
  try {
    po::notify(vm);
    url = boost::url(vm["source-url"].as<std::string>());
    username = vm["source-username"].as<std::string>();
    password = vm["source-password"].as<std::string>();
    hassUrl = boost::url(vm["hass-url"].as<std::string>());
    hassEntityId = vm["hass-entity-id"].as<std::string>();
    hassToken = vm["hass-token"].as<std::string>();
    if (auto it = vm.find("detection-size"); it != vm.end()) {
      const auto raw = it->second.as<std::string>();
      if (raw.empty()) {
        return; // no detection size specified, use default
      }
      if (raw.back() == '%') {
        // percentage specified, save as double
        detectionSize = std::stod(raw.substr(0, raw.size() - 1)) / 100.0;
      } else {
        // integer specified, save as int
        detectionSize = std::stoi(raw);
      }
    }
    if (vm.count("detection-debounce")) {
      detectionDebounce =
          std::chrono::seconds{vm["detection-debounce"].as<int>()};
    }
  } catch (const std::exception &e) {
    std::cout << e.what() << "\n";
    std::cout << desc << "\n";
    exit(1);
  }
}

bool ProgramOptions::CanSetupHass() const {
  return !hassUrl.empty() && !hassEntityId.empty() && !hassToken.empty();
}

} // namespace util