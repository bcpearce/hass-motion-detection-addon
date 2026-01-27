#include "WindowsWrapper.h"

#include "Logger.h"

#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"
#include "VideoSource/Live555.h"
#include "VideoSource/RestartWatcher.h"
#include <BasicUsageEnvironment.hh>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/process.hpp>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>
#include <regex>

#include "LogEnv.h"

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
namespace bp2 = boost::process::v2;
namespace asio = boost::asio;
using json = nlohmann::json;

struct Args {
  std::chrono::seconds duration{10};
  std::filesystem::path rtspServerExec;
  std::filesystem::path motionDetectionExec;
};
Args args;

void StopEventLoop(void *clientData) {
  auto &wv = *static_cast<EventLoopWatchVariable *>(clientData);
  wv.store(1);
}

void StopStream(void *clientData) {
  auto &source = *static_cast<video_source::Live555VideoSource *>(clientData);
  source.StopStream();
}

class RTSPServerFixture : public testing::Test {
public:
  RTSPServerFixture() : stderrCap_{ioCtx_} {
    resourcePath_ = std::filesystem::current_path() / resourceFile;
  }

  void SetUp() override {
    ASSERT_TRUE(
        std::filesystem::exists(std::filesystem::current_path() / resourceFile))
        << "Expected to find resource file " << resourceFile
        << "in working directory";
    ASSERT_TRUE(std::filesystem::is_regular_file(
        std::filesystem::current_path() / resourceFile))
        << "Expected to find resource file 'test.264' in working directory";
    rtspServerProc_ = bp2::process(
        ioCtx_, args.rtspServerExec, {}, bp2::process_stdio{{}, {}, stderrCap_},
        bp2::process_start_dir{std::filesystem::current_path()});

    timer_ = decltype(timer_)(ioCtx_);
    timer_->expires_after(args.duration * 2);
    timer_->async_wait([this](const boost::system::error_code &ec) {
      if (!ec) {
        LOGGER->info("Timeout in ioCtx");
        didTimeout_ = true;
        FAIL() << "Timeout occurred during test";
      } else {
        FAIL() << "Something went wrong " << ec.what();
      }
    });

    doRead();
    while (rtspServerUrl_.empty() && !didTimeout_) {
      ioCtx_.run_one();
    }
    ASSERT_FALSE(didTimeout_)
        << "Timeout occurred setting up RTSP Server Process";
  }

  void TearDown() {
    EXPECT_FALSE(didTimeout_) << "Timeout occurred during test.";
  }

protected:
  static constexpr auto resourceFile{"test.264"sv};

  void doRead() {
    asio::async_read_until(
        stderrCap_, streamBuf_, "\n",
        boost::bind(&RTSPServerFixture::handleLine, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }
  void handleLine(const boost::system::error_code &ec, size_t n) {
    if (!ec) {
      std::istream is(&streamBuf_);
      std::string line;
      std::getline(is, line);
      boost::trim_right(line);
      if (line.ends_with(fmt::format("\"{}\"", resourceFile))) {
        captureUrl_ = true;
      } else if (captureUrl_) {
        const std::regex pattern("\"(rtsps?://[^\\s]+)\"$");
        std::smatch matches;
        if (std::regex_search(line, matches, pattern)) {
          rtspServerUrl_ = boost::url(matches[1].str());
        } else {
          LOGGER->warn("[RTSP SERVER] Resetting URL search");
        }
        captureUrl_ = false;
      }
      LOGGER->info("[RTSP SERVER] {}", line);
      doRead();
    } else if (ec == asio::error::eof) {
      LOGGER->info("[RTSP SERVER] received EOF");
    } else {
      LOGGER->error("[RTSP SERVER] an error occurred {}", ec.what());
    }
  }

  std::filesystem::path resourcePath_;
  asio::io_context ioCtx_;
  asio::streambuf streamBuf_;
  asio::readable_pipe stderrCap_;
  std::optional<asio::steady_timer> timer_;
  std::optional<bp2::process> rtspServerProc_;

  bool captureUrl_{false};
  boost::url rtspServerUrl_;
  bool didTimeout_{false};
};

TEST_F(RTSPServerFixture, Live555VideoSourceTest) {
  auto pSched = std::shared_ptr<TaskScheduler>(BasicTaskScheduler::createNew());
  auto pSource = std::make_shared<video_source::Live555VideoSource>(
      pSched, rtspServerUrl_);

  // Install a restart watcher
  bool didUpdateListener{false};
  struct Callback {
    bool didUpdateListener{false};
    void operator()(int) { didUpdateListener = true; }
  };
  EventLoopWatchVariable wv{0};
  auto spCallback = std::make_shared<Callback>();
  video_source::RestartWatcher<Callback> watcher("Live555", pSource, pSched);
  watcher.wpCallbacks.push_back(spCallback);
  watcher.interval = 1s;
  watcher.minInterval = 1s;
  watcher.maxInterval = 10s;

  asio::post(ioCtx_, [&] {
    pSource->StartStream();
    pSched->scheduleDelayedTask(
        std::chrono::microseconds(args.duration / 2).count(), StopStream,
        pSource.get());
    pSched->scheduleDelayedTask(
        std::chrono::microseconds(args.duration * 3 / 4).count(), StopEventLoop,
        &wv);
    pSched->doEventLoop(&wv);
  });

  ioCtx_.run_for(args.duration);
  EXPECT_GT(pSource->GetFrameCount(), 0);
  EXPECT_TRUE(spCallback->didUpdateListener);
  EXPECT_GT(watcher.GetRestartAttempts(), 0);
  EXPECT_GT(watcher.GetNullPayloadUpdates(), 0);

  RecordProperty("Frame Count", pSource->GetFrameCount());
  RecordProperty("Restart Attempts", watcher.GetRestartAttempts());
  RecordProperty("Null Payload Updates", watcher.GetNullPayloadUpdates());
}

TEST_F(RTSPServerFixture, EndToEnd) {
  const auto config = std::invoke([this] {
    json config;
    config["main"]["sourceUrl"] = rtspServerUrl_.c_str();
    config["main"]["detectionSize"] = 10;
    config["main"]["detectionDebounce"] = 1;
    config["invalid"]["sourceUrl"] = "rtsp://localhost:9090/invalid";
    return config;
  });

  bp2::process motionDetectionProc(
      ioCtx_, args.motionDetectionExec, {"--source-config-raw", config.dump()},
      bp2::process_start_dir{std::filesystem::current_path()});
  ioCtx_.run_for(args.duration);
  motionDetectionProc.request_exit();
  ASSERT_EQ(EXIT_SUCCESS, motionDetectionProc.wait());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (const auto end = arg.find('='); end != std::string_view::npos) {
      const auto prefix = arg.substr(0, end);
      const auto value = arg.substr(prefix.size() + 1);
      if (prefix == "--duration") {
        args.duration =
            std::min(10s, std::chrono::seconds{std::stoi(std::string(value))});
      } else if (prefix == "--rtspServerExec") {
        args.rtspServerExec = std::filesystem::path(value);
      } else if (prefix == "--motionDetectionExec") {
        args.motionDetectionExec = std::filesystem::path(value);
      }
    }
  }
  std::ignore = testing::AddGlobalTestEnvironment(
      std::make_unique<LoggerEnvironment>().release());

  return RUN_ALL_TESTS();
}