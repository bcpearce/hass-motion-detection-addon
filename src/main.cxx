#include "Logger.h"
#include "WindowsWrapper.h"

#include <array>
#include <chrono>
#include <csignal>
#include <format>
#include <iostream>
#include <memory>
#include <ranges>
#include <thread>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <openssl/ssl.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include "Detector/MotionDetector.h"
#include "Gui/WebHandler.h"
#include "HomeAssistant/HassHandler.h"
#include "Util/ProgramOptions.h"
#include "VideoSource/Http.h"
#include "VideoSource/Live555.h"
#include "VideoSource/VideoSource.h"

using namespace std::string_view_literals;
using namespace std::chrono_literals;

namespace {
std::atomic_bool gExitFlag{false};
} // namespace

void SignalHandler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    LOGGER->info("Received signal {} closing stream...", signal);
    gExitFlag.store(true);
  }
}

void App(const util::ProgramOptions &opts) {

  std::shared_ptr<video_source::VideoSource> pSource{nullptr};
  if (opts.url.scheme() == "http"sv || opts.url.scheme() == "https"sv) {
    pSource = std::make_shared<video_source::HttpVideoSource>(
        opts.url, opts.username, opts.password);
  } else if (opts.url.scheme() == "rtsp"sv) {
    pSource = std::make_shared<video_source::Live555VideoSource>(
        opts.url, opts.username, opts.password);
  } else {
    throw std::runtime_error(std::format("Invalid scheme {} for URL",
                                         std::string_view(opts.url.scheme())));
  }

  auto pDetector = std::make_shared<detector::MOGMotionDetector>(
      detector::MOGMotionDetector::Options{.detectionSize =
                                               opts.detectionSize});

  auto onFrameCallback = [pDetector,
                          mask = cv::Mat()](video_source::Frame frame) mutable {
    if (mask.size() != frame.img.size()) {
      mask = cv::Mat::zeros(frame.img.size(), frame.img.type());
      cv::rectangle(mask,
                    cv::Rect(mask.cols * 0.05, mask.rows * 0.08,
                             mask.cols * 0.9, mask.rows * 0.84),
                    cv::Scalar(0xFF), -1);
      pDetector->mask = mask;
    }
    pDetector->FeedFrame(frame);
  };

  pSource->Subscribe(onFrameCallback);

  std::shared_ptr<home_assistant::HassHandler> pHassHandler;
  if (opts.CanSetupHass()) {
    LOGGER->info("Setting up Home Assistant status update for {} hosted at {}",
                 opts.hassEntityId, opts.hassUrl);
    pHassHandler = home_assistant::HassHandler::Create(
        opts.hassUrl, opts.hassToken, opts.hassEntityId);

    pHassHandler->friendlyName = opts.hassFriendlyName;
    pHassHandler->debounceTime = opts.detectionDebounce;

    auto onMotionDetectionCallbackHass =
        [pHassHandler](detector::Payload data) {
          pHassHandler->operator()(data.rois);
        };
    pDetector->Subscribe(onMotionDetectionCallbackHass);
    pHassHandler->Start();
  }

  gui::WebHandler gh(opts.webUiPort, opts.webUiHost);
  LOGGER->info("Web interface started at {}", gh.GetUrl());
  gh.Start();
  auto onMotionDetectorCallbackGui = [&gh, pDetector,
                                      pSource](detector::Payload data) {
    gh({.rois = data.rois,
        .frame = data.frame,
        .detail = pDetector->GetModel(),
        .fps = pSource->GetFramesPerSecond()});
  };
  pDetector->Subscribe(onMotionDetectorCallbackGui);

  pSource->InitStream();

  std::signal(SIGINT, SignalHandler);
  std::signal(SIGTERM, SignalHandler);
  while (!gExitFlag && pSource->IsActive()) {
    std::this_thread::sleep_for(1s);
  }

  (*pHassHandler)();
}

int main(int argc, const char **argv) {
  try {
    auto stdoutLogger = logger::InitStderrLogger();
    auto stderrLogger = logger::InitStdoutLogger();
    const util::ProgramOptions opts(argc, argv, true);
    App(opts);
  } catch (const std::exception &e) {
    ERR_LOGGER->critical(e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}