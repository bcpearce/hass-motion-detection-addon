#ifndef INCLUDE_LOGGER_H
#define INCLUDE_LOGGER_H

#include <string_view>

#include <boost/url.hpp>

#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#define LOGGER spdlog::get("console")
#define ERR_LOGGER spdlog::get("error")

namespace logger {

inline auto InitStdoutLogger() {
  auto logger = spdlog::stderr_color_mt("console");
#if _DEBUG
  logger->set_level(spdlog::level::debug);
#endif
  return logger;
}

inline auto InitStderrLogger() {
  auto logger = spdlog::stderr_color_mt("error");
#if _DEBUG
  logger->set_level(spdlog::level::debug);
#endif
  return logger;
}

} // namespace logger

template <>
struct fmt::formatter<boost::url> : fmt::formatter<std::string_view> {
  auto format(boost::url url,
              format_context &ctx) const -> decltype(ctx.out()) {
    if (!url.password().empty()) {
      url.set_password("********");
    }
    return fmt::format_to(ctx.out(), "{}",
                          std::string_view(url.data(), url.size()));
  }
};

#endif