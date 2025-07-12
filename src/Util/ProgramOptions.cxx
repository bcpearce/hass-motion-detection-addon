#include "Logger.h"
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

ProgramOptions::ProgramOptions(int argc, const char **argv) {
  po::options_description desc("Allowed options");

  desc.add_options()
      /*
       * Meta Options
       */
      ("help", "show help message")
      /**/
      ("version", "show version")
      /*
       * Source RTSP Feed Options
       */
      ("source-url", po::value<std::string>()->required(),
       "target video source URL")
      /**/
      ("source-username", po::value<std::string>()->default_value(""),
       "target video source username for basic or digest auth")
      /**/
      ("source-password", po::value<std::string>()->default_value(""),
       "target video source password for basic or digest auth")
      /**/
      ("source-token", po::value<std::string>()->default_value(""),
       "target video source token authentication")
      /*
       * Home Assistant Handler Options
       */
      ("hass-url", po::value<std::string>()->default_value(""),
       "Home Assistant URL to send detector updates")
      /**/
      ("hass-entity-id", po::value<std::string>()->default_value(""),
       "Home Assistant entity ID to update")
      /**/
      ("hass-friendly-name", po::value<std::string>()->default_value(""),
       "Home Assistant entity Friendly Name (to be sent if the entity ID does "
       "not already exist)")
      /**/
      ("hass-token", po::value<std::string>()->default_value(""),
       "Home Assistant long-lived access token for API auth")
      /*
       * Web User-Interface Options
       */
      ("web-ui-host", po::value<std::string>()->default_value("0.0.0.0"),
       "host to bind the web UI to, defaults to localhost, set to '' to "
       "disable the Web GUI")
      /**/
      ("web-ui-port", po::value<int>()->default_value(32834),
       "port to bind the web UI to, defaults to 32834, set to <= 0 to disable "
       "the Web GUI")
      /*
       * Detection Options
       */
      ("detection-size", po::value<std::string>(),
       "minimum size of a motion detection 'blob' that registers as a region "
       "of interest\n"
       "can be registered as an integer pixel count e.g. '500' or a percentage "
       "of the total pixels e.g. '5%'")
      /**/
      ("detection-debounce", po::value<int>(),
       "minimum time between state changes in detection events (i.e. going "
       "from no motion to motion detected) in seconds")
      /**/;

  po::positional_options_description posOpts;
  posOpts.add("source-url", 1);

  po::variables_map vm;

  po::store(po::command_line_parser(argc, argv)
                .options(desc)
                .positional(posOpts)
                .run(),
            vm);

  const auto parsedEnv =
      po::parse_environment(desc, [](std::string_view envVar) {
        static const std::map<std::string, std::string, std::less<>>
            envVarToProgOpts{{"USERNAME"s, "source-username"s},
                             {"PASSWORD"s, "source-password"s},
                             {"TOKEN"s, "source-token"s},
                             {"URL"s, "source-url"s},
                             {"HASS_URL"s, "hass-url"s},
                             {"HASS_ENTITY_ID"s, "hass-entity-id"s},
                             {"HASS_TOKEN"s, "hass-token"},
                             {"HASS_FRIENDLY_NAME"s, "hass-friendly-name"s},
                             {"WEB_UI_HOST"s, "web-ui-host"s},
                             {"WEB_UI_PORT"s, "web-ui-port"s},
                             {"DETECTION_SIZE"s, "detection-size"s},
                             {"DETECTION_DEBOUNCE"s, "detection-debounce"s}};
        const auto it = envVarToProgOpts.find(envVar);
        return it != envVarToProgOpts.end() ? it->second : ""s;
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
    hassFriendlyName = vm["hass-friendly-name"].as<std::string>();
    hassToken = vm["hass-token"].as<std::string>();
    webUiHost = vm["web-ui-host"].as<std::string>();
    webUiPort = vm["web-ui-port"].as<int>();
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
    if (auto it = vm.find("detection-debounce"); it != vm.end()) {
      detectionDebounce = std::chrono::seconds{it->second.as<int>()};
    }
  } catch (const std::exception &e) {
    std::cout << e.what() << "\n";
    std::cout << desc << std::endl;
    exit(1);
  }
}

bool ProgramOptions::CanSetupHass() const {
  return !hassUrl.empty() && !hassEntityId.empty() && !hassToken.empty();
}

} // namespace util