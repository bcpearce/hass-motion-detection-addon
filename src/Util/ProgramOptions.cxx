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

std::variant<ProgramOptions, std::string>
ProgramOptions::ParseOptions(int argc, const char **argv) {
  /*
   * Meta Options
   */
  po::options_description allOptions("Allowed options");
  allOptions.add_options()
      /**/
      ("help", "show help message")
      /**/
      ("version", "show version");

  /*
   * Source RTSP Feed Options
   */
  po::options_description sourceFeedOptions("Source Feed Options");
  sourceFeedOptions.add_options()
      /**/
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
       "target video source token authentication");
  allOptions.add(sourceFeedOptions);

  /*
   * Home Assistant Handler Options
   */
  po::options_description homeAssistantOptions("Home Assistant Options");
  homeAssistantOptions.add_options()
      /**/
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
       "Home Assistant long-lived access token for API auth");
  allOptions.add(homeAssistantOptions);

  /*
   * Web User-Interface Options
   */
  po::options_description webUiOptions("Web User-Interface Options");
  webUiOptions.add_options()
      /**/
      ("web-ui-host", po::value<std::string>()->default_value("0.0.0.0"),
       "host to bind the web UI to, defaults to localhost, set to '' to "
       "disable the Web GUI")
      /**/
      ("web-ui-port", po::value<int>()->default_value(32834),
       "port to bind the web UI to, defaults to 32834, set to <= 0 to disable "
       "the Web GUI");
  allOptions.add(webUiOptions);

  /*
   * Detection Options
   */
  po::options_description detectionOptions("Detection Options");
  detectionOptions.add_options()
      /**/
      ("detection-size", po::value<std::string>(),
       "minimum size of a motion detection 'blob' that registers as a region "
       "of interest\n"
       "can be registered as an integer pixel count e.g. '500' or a percentage "
       "of the total pixels e.g. '5%'")
      /**/
      ("detection-debounce", po::value<int>(),
       "minimum time between state changes in detection events (i.e. going "
       "from no motion to motion detected) in seconds")
      /**/
      ("save-destination", po::value<std::string>()->default_value(""),
       "destination to save the motion detection video files to, if empty, "
       "video saving is disabled")
      /**/
      ("save-source-url", po::value<std::string>()->default_value(""),
       "URL to obtain image files from, if empty, it will default to using the "
       "decoded video frame. The purpose of this option is to allow for "
       "pulling snapshot images through a different API which may provide "
       "higher fidelity images. This will use the same authentication scheme "
       "as the source URL and should have the same host.")
      /**/
      ("save-image-limit", po::value<size_t>()->default_value(200),
       "maximum number of images to save per motion detection event, default "
       "is 200");
  allOptions.add(detectionOptions);

  po::positional_options_description posOpts;
  posOpts.add("source-url", 1);

  po::variables_map vm;

  po::store(po::command_line_parser(argc, argv)
                .options(allOptions)
                .positional(posOpts)
                .run(),
            vm);

  const auto parsedEnv =
      po::parse_environment(allOptions, [](std::string_view envVar) {
        static const std::map<std::string, std::string, std::less<>>
            envVarToProgOpts{
                {"MODET_SOURCE_USERNAME"s, "source-username"s},
                {"MODET_SOURCE_PASSWORD"s, "source-password"s},
                {"MODET_SOURCE_TOKEN"s, "source-token"s},
                {"MODET_SOURCE_URL"s, "source-url"s},
                {"MODET_HASS_URL"s, "hass-url"s},
                {"MODET_HASS_ENTITY_ID"s, "hass-entity-id"s},
                {"MODET_HASS_TOKEN"s, "hass-token"},
                {"MODET_HASS_FRIENDLY_NAME"s, "hass-friendly-name"s},
                {"MODET_WEB_UI_HOST"s, "web-ui-host"s},
                {"MODET_WEB_UI_PORT"s, "web-ui-port"s},
                {"MODET_DETECTION_SIZE"s, "detection-size"s},
                {"MODET_DETECTION_DEBOUNCE"s, "detection-debounce"s},
                {"MODET_SAVE_DESTINATION"s, "save-destination"s},
                {"MODET_SAVE_SOURCE_URL", "save-source-url"s},
                {"MODET_SAVE_IMAGE_LIMIT", "save-image-limit"s}};
        const auto it = envVarToProgOpts.find(envVar);
        return it != envVarToProgOpts.end() ? it->second : ""s;
      });
  po::store(parsedEnv, vm);

  if (vm.count("help")) {
    std::ostringstream oss;
    oss << allOptions;
    return oss.str();
  }

  if (vm.count("version")) {
    return std::string(MOTION_DETECTION_SEMVER);
  }

  ProgramOptions options;
  try {
    po::notify(vm);
    options.sourceUrl = boost::url(vm["source-url"].as<std::string>());
    options.sourceUsername = vm["source-username"].as<std::string>();
    options.sourcePassword = vm["source-password"].as<std::string>();
    options.sourceToken = vm["source-token"].as<std::string>();

    options.hassUrl = boost::url(vm["hass-url"].as<std::string>());
    options.hassEntityId = vm["hass-entity-id"].as<std::string>();
    options.hassFriendlyName = vm["hass-friendly-name"].as<std::string>();
    options.hassToken = vm["hass-token"].as<std::string>();

    options.webUiHost = vm["web-ui-host"].as<std::string>();
    options.webUiPort = vm["web-ui-port"].as<int>();

    if (auto it = vm.find("detection-size"); it != vm.end()) {
      const auto raw = it->second.as<std::string>();
      if (raw.empty()) {
        ; // no detection size specified, use default
      } else if (raw.back() == '%') {
        // percentage specified, save as double
        options.detectionSize =
            std::stod(raw.substr(0, raw.size() - 1)) / 100.0;
      } else {
        // integer specified, save as int
        options.detectionSize = std::stoi(raw);
      }
    }
    if (auto it = vm.find("detection-debounce"); it != vm.end()) {
      options.detectionDebounce = std::chrono::seconds{it->second.as<int>()};
    }

    options.saveDestination = vm["save-destination"].as<std::string>();
    options.saveSourceUrl = boost::url(vm["save-source-url"].as<std::string>());
    options.saveImageLimit = vm["save-image-limit"].as<size_t>();

  } catch (const std::exception &e) {
    std::ostringstream oss;
    oss << e.what() << "\n" << allOptions;
    return oss.str();
  }
  return options;
}

bool ProgramOptions::CanSetupHass() const {
  return !hassUrl.empty() && !hassEntityId.empty() && !hassToken.empty();
}

} // namespace util