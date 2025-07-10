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
#include "HomeAssistant/AsyncHassHandler.h"
#include "HomeAssistant/SyncHassHandler.h"
#include "HomeAssistant/ThreadedHassHandler.h"
#include "Util/ProgramOptions.h"
#include "VideoSource/Http.h"
#include "VideoSource/Live555.h"
#include "VideoSource/VideoSource.h"

using namespace std::string_view_literals;
using namespace std::chrono_literals;

namespace {
struct ExitSignalHandler {
  std::weak_ptr<video_source::VideoSource> wpSource;
  void operator()(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
      LOGGER->info("Received signal {} closing stream...", signal);
      if (auto spSource = wpSource.lock()) {
        spSource->StopStream();
      }
    }
  }
};

static ExitSignalHandler exitSignalHandler;

void SignalHandlerWrapper(int signal) {
  exitSignalHandler(signal); // Call the functor's operator()
}

} // namespace

void EventLoopSignalHandler(int signal) {}

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

  std::shared_ptr<home_assistant::BaseHassHandler> pHassHandler;
  if (opts.CanSetupHass()) {
    LOGGER->info("Setting up Home Assistant status update for {} hosted at {}",
                 opts.hassEntityId, opts.hassUrl);
    if (std::dynamic_pointer_cast<video_source::HttpVideoSource>(pSource)) {
      // Require Threaded
      LOGGER->info("Running Home Assistant callbacks in separate thread");
      auto pThreadedHassHandler =
          std::make_shared<home_assistant::ThreadedHassHandler>(
              opts.hassUrl, opts.hassToken, opts.hassEntityId);

      pThreadedHassHandler->Start();
      pHassHandler = pThreadedHassHandler;
    } else if (auto pLive555Source =
                   std::dynamic_pointer_cast<video_source::Live555VideoSource>(
                       pSource)) {
      // Optimize with Async
      LOGGER->info("Running Home Assistant callbacks in main event loop");
      auto pAsyncHassHandler =
          std::make_shared<home_assistant::AsyncHassHandler>(
              opts.hassUrl, opts.hassToken, opts.hassEntityId);
      pAsyncHassHandler->Register(pLive555Source->GetTaskSchedulerPtr());
      pHassHandler = pAsyncHassHandler;
    }

    pHassHandler->friendlyName = opts.hassFriendlyName;
    pHassHandler->debounceTime = opts.detectionDebounce;

    auto onMotionDetectionCallbackHass =
        [pHassHandler](detector::Payload data) {
          pHassHandler->operator()(data.rois);
        };
    pDetector->Subscribe(onMotionDetectionCallbackHass);
  }

  std::shared_ptr<gui::WebHandler> pWebHandler;
  if (opts.webUiPort > 0 && !opts.webUiHost.empty()) {
    pWebHandler =
        std::make_shared<gui::WebHandler>(opts.webUiPort, opts.webUiHost);
    LOGGER->info("Web interface started at {}", pWebHandler->GetUrl());
    pWebHandler->Start();
    auto onMotionDetectorCallbackGui = [pWebHandler, pDetector,
                                        pSource](detector::Payload data) {
      (*pWebHandler)({.rois = data.rois,
                      .frame = data.frame,
                      .detail = pDetector->GetModel(),
                      .fps = pSource->GetFramesPerSecond()});
    };
    pDetector->Subscribe(onMotionDetectorCallbackGui);
  }

  exitSignalHandler.wpSource = pSource;

  std::signal(SIGINT, SignalHandlerWrapper);
  std::signal(SIGTERM, SignalHandlerWrapper);

  pSource->StartStream();

  // At this point, performance is no longer critical as the feed is shut down,
  // send the last message using a synchronous handler
  if (opts.CanSetupHass()) {
    home_assistant::SyncHassHandler syncHandler(opts.hassUrl, opts.hassToken,
                                                opts.hassEntityId);
    syncHandler({});
  }
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