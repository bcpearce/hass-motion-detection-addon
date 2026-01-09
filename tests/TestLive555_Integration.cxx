#include "WindowsWrapper.h"

#include <gtest/gtest.h>

#include <BasicUsageEnvironment.hh>

#include "VideoSource/Http.h"
#include "VideoSource/Live555.h"

#include "LogEnv.h"

using namespace std::chrono_literals;

namespace {

namespace args {
static std::chrono::seconds duration{3};
static boost::url targetUrl{"rtsp://172.18.204.53:8554/h264ESVideoTest"};
} // namespace args

void StopStream(void *clientData) {
  auto &wv = *static_cast<EventLoopWatchVariable *>(clientData);
  wv.store(1);
}

} // namespace

TEST(Live555VideoSourceTests, Smoke) {
  auto pSched = std::shared_ptr<TaskScheduler>(BasicTaskScheduler::createNew());
  video_source::Live555VideoSource live555(pSched, args::targetUrl);
  live555.StartStream();
  EventLoopWatchVariable wv;
  pSched->scheduleDelayedTask(std::chrono::microseconds(args::duration).count(),
                              StopStream, &wv);
  pSched->doEventLoop(&wv);
  ASSERT_GT(live555.GetFrameCount(), 0);
}

int main(int argc, char **argv) {
  testing::InitGoogleTest(&argc, argv);

  for (int i = 0; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    const auto prefix = arg.substr(0, arg.find_first_of('='));
    if (prefix == "--duration=") {
      args::duration = decltype(args::duration){
          std::stoi(std::string(arg.substr(prefix.size())))};
    } else if (prefix == "--targetUrl=") {
      args::targetUrl = decltype(args::targetUrl){arg.substr(prefix.size())};
    }
  }

  std::ignore = testing::AddGlobalTestEnvironment(
      std::make_unique<LoggerEnvironment>().release());

  return RUN_ALL_TESTS();
}