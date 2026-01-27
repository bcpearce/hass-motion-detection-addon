#include "Logger.h"
#include "WindowsWrapper.h"

#include "Util/ProgramOptions.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string_view>

#include <boost/program_options.hpp>
#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include <nlohmann/json.hpp>

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
      ("help,h", "show help message")
      /**/
      ("version,v", "show version");

  /*
   * Source RTSP Feed Options
   */
  po::options_description sourceFeedOptions("Sources Config File");
  sourceFeedOptions.add_options()
      /**/
      ("source-config,c", po::value<std::filesystem::path>(),
       "Path to source Config File");

  /*
   * Source RTSP Feed Options
   */
  sourceFeedOptions.add_options()
      /**/
      ("source-config-raw", po::value<std::string>(), "Raw config JSON string");
  allOptions.add(sourceFeedOptions);

  /*
   * Home Assistant Handler Options
   */
  po::options_description homeAssistantOptions("Home Assistant Options");
  homeAssistantOptions.add_options()
      /**/
      ("hass-url,u", po::value<std::string>()->default_value(""),
       "Home Assistant URL to send detector updates")
      /**/
      ("hass-token,t", po::value<std::string>()->default_value(""),
       "Home Assistant long-lived access token for API auth");
  allOptions.add(homeAssistantOptions);

  /*
   * Web User-Interface Options
   */
  po::options_description webUiOptions("Web User-Interface Options");
  webUiOptions.add_options()
      /**/
      ("web-ui-host,s", po::value<std::string>()->default_value("0.0.0.0"),
       "host to bind the web UI to, defaults to localhost, set to '' to "
       "disable the Web GUI")
      /**/
      ("web-ui-port,p", po::value<int>()->default_value(32834),
       "port to bind the web UI to, defaults to 32834, set to <= 0 to disable "
       "the Web GUI");
  allOptions.add(webUiOptions);

  /*
   * Detection Options
   */
  po::options_description detectionOptions("Detection Options");
  detectionOptions.add_options()
      /**/
      ("save-destination", po::value<std::string>()->default_value(""),
       "destination to save the motion detection video files to, if empty, "
       "video saving is disabled");
  allOptions.add(detectionOptions);

  po::variables_map vm;

  po::store(po::command_line_parser(argc, argv).options(allOptions).run(), vm);

  const auto parsedEnv =
      po::parse_environment(allOptions, [](std::string_view envVar) {
        static const std::map<std::string, std::string, std::less<>>
            envVarToProgOpts{{"MODET_HASS_URL"s, "hass-url"s},
                             {"MODET_HASS_TOKEN"s, "hass-token"},
                             {"MODET_WEB_UI_HOST"s, "web-ui-host"s},
                             {"MODET_WEB_UI_PORT"s, "web-ui-port"s},
                             {"MODET_SAVE_DESTINATION"s, "save-destination"s}};
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

    if (!vm["source-config"].empty() && !vm["source-config-raw"].empty()) {
      throw std::invalid_argument(
          "Must specify only one of 'source-config' or 'source-config-raw'");
    } else if (!vm["source-config-raw"].empty()) {
      const auto sourceConfigRaw = vm["source-config-raw"].as<std::string>();
      options.feeds = FeedOptions::ParseJson(std::string_view(sourceConfigRaw));
    } else {
      options.feeds = FeedOptions::ParseJson(
          vm["source-config"].as<std::filesystem::path>());
    }

    options.hassUrl = boost::url(vm["hass-url"].as<std::string>());
    options.hassToken = vm["hass-token"].as<std::string>();

    options.webUiHost = vm["web-ui-host"].as<std::string>();
    options.webUiPort = vm["web-ui-port"].as<int>();

    options.saveDestination = vm["save-destination"].as<std::string>();

  } catch (const std::exception &e) {
    std::ostringstream oss;
    oss << e.what() << "\n" << allOptions;
    return oss.str();
  }
  return options;
}

auto _ParseJson(const nlohmann::json &json)
    -> std::unordered_map<std::string, ProgramOptions::FeedOptions> {

  std::unordered_map<std::string, ProgramOptions::FeedOptions> res;

  for (const auto &[key, value] : json.items()) {
    if (!value.is_object()) {
      LOGGER->error(
          "Invalid feed options for key '{}': expected an object, got {}", key,
          value.type_name());
      continue;
    }
    ProgramOptions::FeedOptions feedOpts;
    if (value.contains("sourceUrl")) {
      feedOpts.sourceUrl =
          boost::url(value["sourceUrl"].template get<std::string>());
    }
    if (value.contains("sourceToken")) {
      feedOpts.sourceToken = value["sourceToken"].template get<std::string>();
    }
    if (value.contains("sourceUsername")) {
      feedOpts.sourceUsername =
          value["sourceUsername"].template get<std::string>();
    }
    if (value.contains("sourcePassword")) {
      feedOpts.sourcePassword =
          value["sourcePassword"].template get<std::string>();
    }
    if (value.contains("hassEntityId")) {
      feedOpts.hassEntityId = value["hassEntityId"].template get<std::string>();
    }
    if (value.contains("hassFriendlyName")) {
      feedOpts.hassFriendlyName =
          value["hassFriendlyName"].template get<std::string>();
    }
    if (value.contains("detectionSize")) {
      if (value["detectionSize"].is_string()) {
        const auto rawSize = value["detectionSize"].template get<std::string>();
        if (rawSize.back() == '%') {
          feedOpts.detectionSize =
              std::stod(rawSize.substr(0, rawSize.size() - 1)) / 100.0;
        } else {
          feedOpts.detectionSize = std::stoi(rawSize);
        }
      } else if (value["detectionSize"].is_number_integer()) {
        feedOpts.detectionSize = value["detectionSize"].template get<int>();
      }
    }
    if (value.contains("detectionDebounce")) {
      feedOpts.detectionDebounce =
          std::chrono::seconds{value["detectionDebounce"].template get<int>()};
    }
    if (value.contains("saveSourceUrl")) {
      feedOpts.saveSourceUrl =
          boost::url(value["saveSourceUrl"].template get<std::string>());
    }
    if (value.contains("saveImageLimit")) {
      feedOpts.saveImageLimit = value["saveImageLimit"].template get<size_t>();
    }
    res[key] = std::move(feedOpts);
  }

  return res;
}

auto ProgramOptions::FeedOptions::ParseJson(const std::filesystem::path &json)
    -> std::unordered_map<std::string, FeedOptions> {
  std::ifstream file(json);
  const nlohmann::json data = nlohmann::json::parse(file);
  return _ParseJson(data);
}

auto ProgramOptions::FeedOptions::ParseJson(std::string_view jsonSv)
    -> std::unordered_map<std::string, FeedOptions> {
  const nlohmann::json data = nlohmann::json::parse(jsonSv);
  return _ParseJson(data);
}

bool ProgramOptions::CanSetupHass(const FeedOptions &feedOpts) const {
  return !hassUrl.empty() && !feedOpts.hassEntityId.empty() &&
         !hassToken.empty();
}

bool ProgramOptions::CanSetupFileSave(const FeedOptions &feedOpts) const {
  return !saveDestination.empty() && !feedOpts.saveSourceUrl.empty();
}

} // namespace util