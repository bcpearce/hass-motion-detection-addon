#include "WindowsWrapper.h"

#include "Logger.h"

#include "Callback/BaseHassHandler.h"
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
#include <gtest/gtest.h>
#include <openssl/sha.h>
#include <regex>

#include "LogEnv.h"

using namespace std::chrono_literals;
using namespace std::string_literals;
using namespace std::string_view_literals;
namespace bp2 = boost::process::v2;
namespace asio = boost::asio;

struct Args {
  std::chrono::seconds duration;
  std::filesystem::path rtspServerExec;
};
Args args;

void StopStream(void *clientData) {
  auto &wv = *static_cast<EventLoopWatchVariable *>(clientData);
  wv.store(1);
}

class Live555VideoSourceTests : public testing::Test {
public:
  Live555VideoSourceTests() : stderrCap_{ioCtx_} {
    resourcePath_ = std::filesystem::current_path() / resourceFile;
  }

protected:
  static constexpr auto resourceFile{"test.264"sv};

  void doRead() {
    asio::async_read_until(
        stderrCap_, streamBuf_, "\n",
        boost::bind(&Live555VideoSourceTests::handleLine, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
  }
  void handleLine(const boost::system::error_code &ec, size_t n) {
    if (!ec) {
      std::istream is(&streamBuf_);
      std::string line;
      std::getline(is, line);
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
    } else {
      LOGGER->error("[RTSP SERVER] an error occurred {}", ec.what());
    }
  }
  std::filesystem::path resourcePath_;
  asio::io_context ioCtx_;
  asio::streambuf streamBuf_;
  asio::readable_pipe stderrCap_;

  bool captureUrl_{false};
  boost::url rtspServerUrl_;
};

TEST_F(Live555VideoSourceTests, Smoke) {
  ASSERT_TRUE(
      std::filesystem::exists(std::filesystem::current_path() / resourceFile))
      << "Expected to find resource file " << resourceFile
      << "in working directory";
  ASSERT_TRUE(std::filesystem::is_regular_file(std::filesystem::current_path() /
                                               resourceFile))
      << "Expected to find resource file 'test.264' in working directory";
  bp2::async_execute(bp2::process(ioCtx_, args.rtspServerExec, {},
                                  bp2::process_stdio{{}, {}, stderrCap_}))(
      asio::cancel_after(args.duration, asio::cancellation_type::partial))(
      asio::cancel_after(args.duration, asio::cancellation_type::terminal))(
      asio::detached);

  bool didTimeout{false};
  asio::steady_timer timer(ioCtx_);
  timer.expires_after(args.duration);
  timer.async_wait([&didTimeout](const boost::system::error_code &ec) {
    if (!ec) {
      LOGGER->info("Timeout in ioCtx");
    } else {
      LOGGER->error("Timer error {}", ec.what());
    }
    didTimeout = true;
  });

  doRead();
  while (rtspServerUrl_.empty() && !didTimeout) {
    ioCtx_.run_one();
  }

  auto pSched = std::shared_ptr<TaskScheduler>(BasicTaskScheduler::createNew());
  auto pSource = std::make_shared<video_source::Live555VideoSource>(
      pSched, rtspServerUrl_);

  // Install a restart watcher
  video_source::RestartWatcher<callback::BaseHassHandler> watcher(
      "Live555", pSource, pSched);
  watcher.interval = 1s;
  watcher.minInterval = 1s;
  watcher.maxInterval = 10s;

  asio::post(ioCtx_, [&] {
    pSource->StartStream();
    EventLoopWatchVariable wv;
    pSched->scheduleDelayedTask(
        std::chrono::microseconds(args.duration).count(), StopStream, &wv);
    pSched->doEventLoop(&wv);
  });

  ioCtx_.run();
  EXPECT_GT(pSource->GetFrameCount(), 0);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (const auto end = arg.find('='); end != std::string_view::npos) {
      const auto prefix = arg.substr(0, end);
      const auto value = arg.substr(prefix.size() + 1);
      if (prefix == "--duration") {
        args.duration = std::chrono::seconds{std::stoi(std::string(value))};
      } else if (prefix == "--rtspServerExec") {
        args.rtspServerExec = std::filesystem::path(value);
      }
    }
  }
  std::ignore = testing::AddGlobalTestEnvironment(
      std::make_unique<LoggerEnvironment>().release());

  return RUN_ALL_TESTS();
}