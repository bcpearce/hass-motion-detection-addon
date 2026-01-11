#include "WindowsWrapper.h"

#include "Logger.h"

#include "Util/BufferOperations.h"
#include "Util/CurlWrapper.h"
#include "VideoSource/Http.h"
#include "VideoSource/Live555.h"
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
namespace bp2 = boost::process::v2;
namespace asio = boost::asio;

struct Args {
  std::chrono::seconds duration;
  boost::url resourceUrl;
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
    std::vector<std::string> splits;
    boost::split(splits, args.resourceUrl.c_str(), boost::is_any_of("/"));
    resourcePath_ = splits.back();
    boost::split(splits, resourcePath_.string(), boost::is_any_of("."));
    if (splits.front() != "test"s) {
      resourcePath_ = std::format("test.{}", splits.back());
    }
    shaPath_ = std::format("{}.sha.txt", splits.back());
  }

  void SetUp() override {
    if (std::filesystem::exists(resourcePath_)) {
      LOGGER->info("Resource at {} already exists", resourcePath_.string());
      return;
    }
    std::filesystem::remove(resourcePath_);
    pResourceFile_ = fopen(resourcePath_.string().c_str(), "wb");
    pShaFile_ = fopen(shaPath_.string().c_str(), "w");
    if (pResourceFile_) {
      const std::vector<char> buf = std::invoke([] {
        util::CurlWrapper wCurl;
        wCurl(curl_easy_setopt, CURLOPT_URL, args.resourceUrl.c_str());
        std::vector<char> _buf;
        wCurl(curl_easy_setopt, CURLOPT_WRITEDATA, &_buf);
        wCurl(curl_easy_setopt, CURLOPT_WRITEFUNCTION,
              util::FillBufferCallback);
        wCurl(curl_easy_perform);
        return _buf;
      });
      if (buf.empty()) {
        FAIL() << "Failed to acquire resource video file at "
               << args.resourceUrl;
      }
      std::fwrite(buf.data(), sizeof buf.front(), buf.size(), pResourceFile_);
      fclose(pResourceFile_);
      pResourceFile_ = nullptr;

      std::array<unsigned char, SHA256_DIGEST_LENGTH> shaHash;
      const unsigned char *bufPtr =
          reinterpret_cast<const unsigned char *>(buf.data());
      SHA256(bufPtr, buf.size(), shaHash.data());
      std::fwrite(shaHash.data(), sizeof shaHash[0], SHA256_DIGEST_LENGTH,
                  pShaFile_);
      fclose(pShaFile_);
      pShaFile_ = nullptr;

      LOGGER->info("Saved {} to {} for test", args.resourceUrl,
                   resourcePath_.string());
    } else {
      FAIL() << "Failed to collect resource file at " << args.resourceUrl;
    }
  }
  void TearDown() override {
    if (pResourceFile_) {
      fclose(pResourceFile_);
    }
    if (pShaFile_) {
      fclose(pShaFile_);
    }
    std::filesystem::remove(resourcePath_);
  }

protected:
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
      if (line.ends_with(fmt::format("\"{}\"", resourcePath_.string()))) {
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
  FILE *pResourceFile_{nullptr};
  FILE *pShaFile_{nullptr};
  std::filesystem::path resourcePath_;
  std::filesystem::path shaPath_;

  asio::io_context ioCtx_;
  asio::streambuf streamBuf_;
  asio::readable_pipe stderrCap_;

  bool captureUrl_{false};
  boost::url rtspServerUrl_;
};

TEST_F(Live555VideoSourceTests, Smoke) {
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
  LOGGER->info("Looking for {} file", resourcePath_.string());

  doRead();
  while (rtspServerUrl_.empty() && !didTimeout) {
    ioCtx_.run_one();
  }

  auto pSched = std::shared_ptr<TaskScheduler>(BasicTaskScheduler::createNew());
  video_source::Live555VideoSource live555(pSched, rtspServerUrl_);
  asio::post(ioCtx_, [&] {
    live555.StartStream();
    EventLoopWatchVariable wv;
    pSched->scheduleDelayedTask(
        std::chrono::microseconds(args.duration).count(), StopStream, &wv);
    pSched->doEventLoop(&wv);
  });

  ioCtx_.run();
  EXPECT_GT(live555.GetFrameCount(), 0);
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
      } else if (prefix == "--resourceUrl") {
        args.resourceUrl = boost::url{value};
      } else if (prefix == "--rtspServerExec") {
        args.rtspServerExec = std::filesystem::path(value);
      }
    }
  }
  std::ignore = testing::AddGlobalTestEnvironment(
      std::make_unique<LoggerEnvironment>().release());

  return RUN_ALL_TESTS();
}