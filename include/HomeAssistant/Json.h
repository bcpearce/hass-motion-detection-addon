#ifndef INCLUDE_HOME_ASSISTANT_JSON_H
#define INCLUDE_HOME_ASSISTANT_JSON_H

#include <nlohmann/json.hpp>
#include <opencv2/core/types.hpp>

namespace home_assistant {

inline void to_json(nlohmann::json &j, const cv::Rect &rect) {
  j = nlohmann::json{{"x", rect.x},
                     {"y", rect.y},
                     {"width", rect.width},
                     {"height", rect.height}};
}

inline void from_json(const nlohmann::json &j, cv::Rect &rect) {
  rect.x = j.at("x").get<int>();
  rect.y = j.at("y").get<int>();
  rect.width = j.at("width").get<int>();
  rect.height = j.at("height").get<int>();
}

} // namespace home_assistant

#endif