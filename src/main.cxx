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

#include <BasicUsageEnvironment.hh>

#include "Callback/AsyncFileSave.h"
#include "Callback/AsyncHassHandler.h"
#include "Callback/SyncHassHandler.h"
#include "Callback/ThreadedHassHandler.h"
#include "Detector/MotionDetector.h"
#include "Gui/WebHandler.h"
#include "Util/ProgramOptions.h"
#include "VideoSource/Http.h"
#include "VideoSource/Live555.h"
#include "VideoSource/RestartWatcher.h"
#include "VideoSource/VideoSource.h"

using namespace std::string_view_literals;
using namespace std::chrono_literals;

namespace {
struct ExitSignalHandler {
  EventLoopWatchVariable watchVar{0};
  void operator()(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
      LOGGER->info("Received signal {} closing stream...", signal);
      watchVar.store(1); // Trigger the event loop to exit
    }
  }
};

struct SourceAndHandlers {
  std::shared_ptr<video_source::VideoSource> pSource;
  std::shared_ptr<detector::MOGMotionDetector> pDetector;
  std::shared_ptr<callback::BaseHassHandler> pHassHandler;
  std::shared_ptr<callback::AsyncFileSave> pFileSaveHandler;
  std::unique_ptr<video_source::RestartWatcher<callback::BaseHassHandler>>
      pRestartWatcher;
};

static ExitSignalHandler exitSignalHandler;

#ifdef _WIN32
BOOL WINAPI SignalHandlerWrapper(DWORD dwCtrlType) {
  switch (dwCtrlType) {
  case CTRL_C_EVENT:
  case CTRL_BREAK_EVENT:
    exitSignalHandler(SIGINT);
    return TRUE;
  case CTRL_CLOSE_EVENT:
    exitSignalHandler(SIGTERM);
    return TRUE;
  default:
    return FALSE;
  }
}
#elifdef __linux__
void SignalHandlerWrapper(int signal) {
  exitSignalHandler(signal); // Call the functor's operator()
}
#endif

} // namespace

void App(const util::ProgramOptions &opts) {

  std::shared_ptr<TaskScheduler> pSched =
      std::shared_ptr<TaskScheduler>(BasicTaskScheduler::createNew());

  std::shared_ptr<gui::WebHandler> pWebHandler;
  if (opts.webUiPort > 0 && !opts.webUiHost.empty()) {
    pWebHandler =
        std::make_shared<gui::WebHandler>(opts.webUiPort, opts.webUiHost);
    LOGGER->info("Web interface started at {}", pWebHandler->GetUrl());
    pWebHandler->Start();
  }

  std::vector<SourceAndHandlers> sources;

  for (const auto &[feedId, feedOpts] : opts.feeds) {
    LOGGER->info("Processing feed: {}", feedId);

    std::shared_ptr<video_source::VideoSource> pSource{nullptr};
    if (feedOpts.sourceUrl.scheme() == "http"sv ||
        feedOpts.sourceUrl.scheme() == "https"sv) {
      pSource = std::make_shared<video_source::HttpVideoSource>(
          feedOpts.sourceUrl, feedOpts.sourceUsername, feedOpts.sourcePassword);
    } else if (feedOpts.sourceUrl.scheme() == "rtsp"sv) {
      auto pLive555Source = std::make_shared<video_source::Live555VideoSource>(
          pSched, feedOpts.sourceUrl, feedOpts.sourceUsername,
          feedOpts.sourcePassword);
      pSource = pLive555Source;
    } else {
      LOGGER->error(std::format("Invalid scheme {} for URL",
                                std::string_view(feedOpts.sourceUrl.scheme())));
      continue;
    }
    sources.push_back(
        {.pSource = pSource,
         .pRestartWatcher = std::make_unique<
             video_source::RestartWatcher<callback::BaseHassHandler>>(
             std::format("Source-{}", sources.size()), pSource, pSched)});

    auto pDetector = std::make_shared<detector::MOGMotionDetector>(
        detector::MOGMotionDetector::Options{.detectionSize =
                                                 feedOpts.detectionSize});

    auto onFrameCallback =
        [pDetector, mask = cv::Mat()](video_source::Frame frame) mutable {
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
    sources.back().pDetector = pDetector;

    std::shared_ptr<callback::BaseHassHandler> pHassHandler;
    if (opts.CanSetupHass(feedOpts)) {
      LOGGER->info(
          "Setting up Home Assistant status update for {} hosted at {}",
          feedOpts.hassEntityId, opts.hassUrl);
      if (std::dynamic_pointer_cast<video_source::HttpVideoSource>(pSource)) {
        // Require Threaded
        LOGGER->info("Running Home Assistant callbacks in separate thread");
        auto pThreadedHassHandler =
            std::make_shared<callback::ThreadedHassHandler>(
                opts.hassUrl, opts.hassToken, feedOpts.hassEntityId);

        pThreadedHassHandler->Start();
        pHassHandler = pThreadedHassHandler;
      } else if (pSched) {
        // Optimize with Async
        LOGGER->info("Running Home Assistant callbacks in main event loop");
        auto pAsyncHassHandler = std::make_shared<callback::AsyncHassHandler>(
            pSched, opts.hassUrl, opts.hassToken, feedOpts.hassEntityId);
        pAsyncHassHandler->Register();
        pHassHandler = pAsyncHassHandler;
      }

      pHassHandler->friendlyName = feedOpts.hassFriendlyName;
      pHassHandler->debounceTime = feedOpts.detectionDebounce;

      auto onMotionDetectionCallbackHass =
          [pHassHandler](detector::Payload data) {
            pHassHandler->operator()(data.rois);
          };
      pDetector->Subscribe(onMotionDetectionCallbackHass);
      sources.back().pHassHandler = pHassHandler;
      sources.back().pRestartWatcher->wpCallbacks.push_back(pHassHandler);
    }

    std::shared_ptr<callback::AsyncFileSave> pFileSaveHandler;
    if (opts.CanSetupFileSave(feedOpts)) {
      if (pSched) {
        pFileSaveHandler = std::make_shared<callback::AsyncFileSave>(
            pSched, opts.saveDestination / feedId, feedOpts.saveSourceUrl,
            feedOpts.sourceUsername, feedOpts.sourcePassword);
        pFileSaveHandler->Register();
        pFileSaveHandler->SetLimitSavedFilePaths(feedOpts.saveImageLimit);
        auto onMotionDetectionCallbackSave =
            [pFileSaveHandler](detector::Payload data) {
              (*pFileSaveHandler)(data);
            };
        pDetector->Subscribe(onMotionDetectionCallbackSave);
        LOGGER->info("Saving motion detection images to {}",
                     opts.saveDestination / feedId);
        sources.back().pFileSaveHandler = pFileSaveHandler;
      }
    }

    if (pWebHandler) {
      if (pFileSaveHandler) {
        gui::WebHandler::SetSavedFilesServePath(feedId,
                                                pFileSaveHandler->GetDstPath());
      }
      auto onMotionDetectorCallbackGui = [pWebHandler, pDetector, pSource,
                                          &feedId](detector::Payload data) {
        (*pWebHandler)({.rois = data.rois,
                        .frame = data.frame,
                        .detail = pDetector->GetModel(),
                        .fps = pSource->GetFramesPerSecond(),
                        .feedId = feedId});
      };
      pDetector->Subscribe(onMotionDetectorCallbackGui);
    }
  }

#ifdef _WIN32
  SetConsoleCtrlHandler(SignalHandlerWrapper, TRUE);
#elifdef __linux__
  std::signal(SIGINT, SignalHandlerWrapper);
  std::signal(SIGTERM, SignalHandlerWrapper);
#endif
  for (auto pSource :
       sources | std::views::transform(&SourceAndHandlers::pSource)) {
    if (pSource) {
      pSource->StartStream();
    }
  }

  pSched->doEventLoop(&exitSignalHandler.watchVar);

  for (auto pSource :
       sources | std::views::transform(&SourceAndHandlers::pSource)) {
    if (pSource) {
      pSource->StopStream();
    }
  }

  // At this point, performance is no longer critical as the feed is shut
  // down, send the last message using a synchronous handler
  for (auto pHassHandler :
       sources | std::views::transform(&SourceAndHandlers::pHassHandler)) {
    if (pHassHandler) {
      callback::SyncHassHandler syncHandler(opts.hassUrl, opts.hassToken,
                                            pHassHandler->entityId);
      syncHandler.friendlyName = pHassHandler->friendlyName;
      syncHandler({});
    }
  }
}

int main(int argc, const char **argv) {
  try {
    auto stdoutLogger = logger::InitStderrLogger();
    auto stderrLogger = logger::InitStdoutLogger();
    const auto optsVar = util::ProgramOptions::ParseOptions(argc, argv);
    if (std::holds_alternative<std::string>(optsVar)) {
      std::cout << std::get<std::string>(optsVar) << "\n";
    } else {
      App(std::get<util::ProgramOptions>(optsVar));
    }
  } catch (const std::exception &e) {
    ERR_LOGGER->critical(e.what());
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}