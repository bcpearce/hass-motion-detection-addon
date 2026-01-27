#include "Logger.h"
#include "WindowsWrapper.h"

#include <BasicUsageEnvironment.hh>

#include "VideoSource/Live555.h"

#include <format>
#include <iostream>
#include <string_view>

#include <liveMedia.hh>
#include <opencv2/imgproc.hpp>
#include <wels/codec_api.h>

using namespace std::string_view_literals;

namespace {
[[nodiscard]] std::string MakeDecoderError(DECODING_STATE state, unsigned int) {
  thread_local std::vector<std::string_view> errors;
  errors.clear();
  if (state & dsFramePending) {
    errors.push_back("FramePending"sv);
  }
  if (state & dsRefLost) {
    errors.push_back("RefLost"sv);
  }
  if (state & dsBitstreamError) {
    errors.push_back("BitstreamError"sv);
  }
  if (state & dsDepLayerLost) {
    errors.push_back("DepLayerLost"sv);
  }
  if (state & dsNoParamSets) {
    errors.push_back("NoParamSets"sv);
  }
  if (state & dsDataErrorConcealed) {
    errors.push_back("DataErrorConcealed"sv);
  }
  if (state & dsRefListNullPtrs) {
    errors.push_back("RefListNullPtrs"sv);
  }
  if (state & dsInvalidArgument) {
    errors.push_back("InvalidArgument"sv);
  }
  if (state & dsInitialOptExpected) {
    errors.push_back("InitialOptExpected"sv);
  }
  if (state & dsOutOfMemory) {
    errors.push_back("OutOfMemory"sv);
  }
  if (state & dsDstBufNeedExpan) {
    errors.push_back("DstBufNeedExpan"sv);
  }
  std::stringstream ss;
  for (size_t i = 0; auto error : errors) {
    ss << error << (++i < errors.size() ? ", "sv : ""sv);
  }

  return std::format("Errors decoding stream ({:X}): {}"sv, int(state),
                     ss.str());
}
} // namespace

template <>
struct fmt::formatter<RTSPClient> : fmt::formatter<std::string_view> {
  auto format(const RTSPClient &rtspClient, format_context &ctx) const
      -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "[RTSP URL: {}]",
                          boost::url(rtspClient.url()));
  }
};

template <>
struct fmt::formatter<MediaSubsession> : fmt::formatter<std::string_view> {
  auto format(const MediaSubsession &subsession, format_context &ctx) const
      -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}/{}", subsession.mediumName(),
                          subsession.codecName());
  }
};

namespace video_source {

// RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode,
                           char *resultString);
void continueAfterSETUP(RTSPClient *rtspClient, int resultCode,
                        char *resultString);
void continueAfterPLAY(RTSPClient *rtspClient, int resultCode,
                       char *resultString);

// Other event handler functions:
void subsessionAfterPlaying(
    void *clientData); // called when a stream's subsession (e.g., audio or
                       // video substream) ends
void subsessionByeHandler(void *clientData, const char *reason);
// called when a RTCP "BYE" is received for a subsession
void streamWatchdogHandler(void *clientData);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient *rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient *rtspClient);

struct StreamClientState {
  std::unique_ptr<MediaSubsessionIterator> iter{nullptr};
  MediaSession *session{nullptr};
  MediaSubsession *subsession{nullptr};
  VideoSource *pVideoSource{nullptr};
  TaskToken streamTimerTask{nullptr};
  double duration{0.0};

  ~StreamClientState() {
    if (session) {
      UsageEnvironment &env = session->envir();
      env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
      Medium::close(session);
    }
  }
};

class FrameRtspClient : public RTSPClient {
protected:
  struct Token {};

public:
  static std::unique_ptr<FrameRtspClient>
  createNew(UsageEnvironment &env, Live555VideoSource &videoSource,
            int verbosityLevel = 0, const char *applicationName = NULL,
            portNumBits tunnelOverHTTPPortNum = 0) {
    return std::make_unique<FrameRtspClient>(Token(), env, videoSource,
                                             verbosityLevel, applicationName,
                                             tunnelOverHTTPPortNum);
  }

  FrameRtspClient(Token, UsageEnvironment &env, Live555VideoSource &videoSource,
                  int verbosityLevel, const char *applicationName,
                  portNumBits tunnelOverHTTPPortNum)
      : RTSPClient(env, videoSource.url_.c_str(), verbosityLevel,
                   applicationName, tunnelOverHTTPPortNum, -1),
        rVideoSource_(videoSource) {}

  ~FrameRtspClient() override {}

  Live555VideoSource &rVideoSource_;
  StreamClientState scs;

  // Set up a watchdog timer to make sure the stream is receiving timely
  // updates, otherwise stop waiting and close
  std::chrono::microseconds waitForFrame{10'000'000};
  std::chrono::steady_clock::time_point lastUpdate{};
};
} // namespace video_source

template <>
struct fmt::formatter<video_source::FrameRtspClient>
    : fmt::formatter<std::string_view> {
  auto format(const video_source::FrameRtspClient &rtspClient,
              format_context &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "[RTSP URL: {}]",
                          boost::url(rtspClient.url()));
  }
};

namespace video_source {

class FrameSetterSink : public MediaSink {
public:
  static FrameSetterSink *CreateNew(UsageEnvironment &env,
                                    MediaSubsession &subsession,
                                    Live555VideoSource &videoSource,
                                    size_t bufferSize = 2'000'000) {
    return new FrameSetterSink(env, subsession, videoSource, bufferSize);
  }

  ~FrameSetterSink() override {
    if (pSvcDecoder_) {
      pSvcDecoder_->Uninitialize();
      WelsDestroyDecoder(pSvcDecoder_);
    }
  }

private:
  FrameSetterSink(UsageEnvironment &env, MediaSubsession &subsession,
                  Live555VideoSource &videoSource, size_t bufferSize)
      : MediaSink(env), receiveBuffer_(bufferSize, 0), rSubsession_(subsession),
        rVideoSource_(videoSource) {

    if (receiveBuffer_.size() < 3) {
      throw std::runtime_error(
          "Receive buffer is not large enough for prefix bytes");
    }
    receiveBuffer_[0] = 0x00;
    receiveBuffer_[1] = 0x00;
    receiveBuffer_[2] = 0x01;

    // Setup the codec
    if (const int res = WelsCreateDecoder(&pSvcDecoder_); res != 0) {
      throw std::runtime_error(
          std::format("Failed to create code with error code {}", res));
    }
    memset(&sDstBufInfo_, 0, sizeof(SBufferInfo));
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
    sDecParam_ = {0};

    sDecParam_.uiTargetDqLayer = (uint8_t)-1;
    sDecParam_.eEcActiveIdc = ERROR_CON_SLICE_COPY;
    sDecParam_.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;

    pSvcDecoder_->Initialize(&sDecParam_);
  }

  void AfterGettingFrame(unsigned int frameSize, unsigned int numTruncatedBytes,
                         timeval presentationTime, unsigned int) {
#ifdef _DEBUG
    const std::string truncatedMsg =
        (numTruncatedBytes > 0)
            ? std::format(" (with {} bytes truncated)", numTruncatedBytes)
            : "";
    const std::string_view syncMarker =
        (rSubsession_.rtpSource() &&
         !rSubsession_.rtpSource()->hasBeenSynchronizedUsingRTCP())
            ? "!"sv
            : ""sv;

    const auto rtspClientRepr =
        rVideoSource_.pRtspClient_
            ? fmt::format("{}", *rVideoSource_.pRtspClient_)
            : "<none>";

    LOGGER->debug("{} {} ({}):\tReceived {} bytes{} \tPresentation time: "
                  "{}.{:06d}{}\tNPT: {}",
                  rtspClientRepr, rSubsession_,
                  rVideoSource_.GetCurrentFrame().id + 1, frameSize,
                  truncatedMsg, presentationTime.tv_sec,
                  presentationTime.tv_usec, syncMarker,
                  rSubsession_.getNormalPlayTime(presentationTime));
#endif
    memset(&sDstBufInfo_, 0, sizeof(SBufferInfo));
    pDataYUV_[0] = pDataYUV_[1] = pDataYUV_[2] = nullptr;
    const auto res = pSvcDecoder_->DecodeFrameNoDelay(
        receiveBuffer_.data(), frameSize + 3, pDataYUV_, &sDstBufInfo_);

    constexpr unsigned int errMask =
        dsBitstreamError | dsNoParamSets | dsDepLayerLost;
    if (res != 0 && (res & errMask)) {
      LOGGER->warn(MakeDecoderError(res, errMask));
    }

    if (sDstBufInfo_.iBufferStatus == 1) {
      const int width = sDstBufInfo_.UsrData.sSystemBuffer.iWidth;
      const int height = sDstBufInfo_.UsrData.sSystemBuffer.iHeight;
      const int timeStamp = sDstBufInfo_.uiOutYuvTimeStamp;
      rVideoSource_.SetYUVFrame(pDataYUV_, width, height,
                                sDstBufInfo_.UsrData.sSystemBuffer.iStride[0],
                                sDstBufInfo_.UsrData.sSystemBuffer.iStride[1],
                                timeStamp);
    }

    if (rVideoSource_.GetFrameCount() < rVideoSource_.maxFrames_) {
      continuePlaying();
    } else {
      rVideoSource_.StopStream();
    }
  }

  static void AfterGettingFrame(void *clientData, unsigned int frameSize,
                                unsigned int numTruncatedBytes,
                                timeval presentationTime,
                                unsigned int durationInMicroseconds) {
    FrameSetterSink *pSink = static_cast<FrameSetterSink *>(clientData);
    pSink->AfterGettingFrame(frameSize, numTruncatedBytes, presentationTime,
                             durationInMicroseconds);
  }

  Boolean continuePlaying() override {
    if (!fSource) {
      return False;
    }
    rVideoSource_.pRtspClient_->lastUpdate = std::chrono::steady_clock::now();
    fSource->getNextFrame(receiveBuffer_.data() + 3, receiveBuffer_.size() - 3,
                          AfterGettingFrame, this, onSourceClosure, this);
    return True;
  }

  DECODING_STATE res{dsBitstreamError};
  ISVCDecoder *pSvcDecoder_{nullptr};
  unsigned char *pDataYUV_[3]{nullptr, nullptr, nullptr};
  SBufferInfo sDstBufInfo_;
  SDecodingParam sDecParam_{};

  std::vector<u_int8_t> receiveBuffer_;
  unsigned int frameCount_{0};
  MediaSubsession &rSubsession_;
  Live555VideoSource &rVideoSource_;
};

Live555VideoSource::Live555VideoSource(std::shared_ptr<TaskScheduler> pSched,
                                       const boost::url &url,
                                       std::string_view username,
                                       std::string_view password)
    : pSched_{pSched}, url_{url} {
  if (!username.empty() && !password.empty()) {
    url_.set_user(username);
    url_.set_password(password);
  }
}

Live555VideoSource::~Live555VideoSource() { StopStream_Impl(); }

void Live555VideoSource::StartStream(unsigned long long maxFrames) {
  if (url_.empty()) {
    throw std::runtime_error("No URL specified");
  }

  pEnv_ = BasicUsageEnvironment::createNew(*pSched_);

// Open URL and establish connection
#ifdef _DEBUG
  static constexpr int VERBOSITY_LEVEL = 1;
#else
  static constexpr int VERBOSITY_LEVEL = 0;
#endif
  pRtspClient_ = FrameRtspClient::createNew(*pEnv_, *this, VERBOSITY_LEVEL,
                                            "Motion-Detector");
  pRtspClient_->sendDescribeCommand(continueAfterDESCRIBE);
  maxFrames_ = maxFrames;
}

void Live555VideoSource::StopStream() { StopStream_Impl(); }

void Live555VideoSource::SetYUVFrame(uint8_t **pDataYUV, int width, int height,
                                     int strideY, int strideUV, int) {

  thread_local cv::Mat Y, U, V, YUV;
  Y = cv::Mat(cv::Size(width, height), CV_8UC1, pDataYUV[0],
              strideY); // non-owning buffer, set to thread local to persist
                        // outside scope

  if (this->fullColor) {
    const cv::Mat U2(cv::Size(width / 2, height / 2), CV_8UC1, pDataYUV[1],
                     strideUV);
    const cv::Mat V2(cv::Size(width / 2, height / 2), CV_8UC1, pDataYUV[2],
                     strideUV);

    thread_local cv::Mat U, V, YUV;
    cv::resize(U2, U, Y.size());
    cv::resize(V2, V, Y.size());
  } else {
    U = cv::Mat();
    V = cv::Mat();
    YUV = cv::Mat();
  }

  try {
    auto frame = GetCurrentFrame();

    if (this->fullColor) {
      cv::merge(std::array{Y, U, V}, YUV);
      cv::cvtColor(YUV, frame.img, cv::COLOR_YUV2BGR);
    } else {
      if (!Y.empty()) {
        frame.img = Y;
      }
    }
    ++frame.id;
    frame.timeStamp = std::chrono::steady_clock::now();
    this->SetFrame(frame);
  } catch (const std::exception &e) {
    LOGGER->error(e.what());
  }
}

void Live555VideoSource::StopStream_Impl() {
  if (pRtspClient_) {
    shutdownStream(pRtspClient_.release());
  }
  if (pEnv_) {
    pEnv_->reclaim();
    pEnv_ = nullptr;
  }
}

void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode,
                           char *resultString) {
  auto upResultString = std::unique_ptr<char[]>(resultString);
  auto *frameRtspClient = dynamic_cast<FrameRtspClient *>(rtspClient);
  if (!frameRtspClient) {
    return;
  }
  do {
    UsageEnvironment &env = frameRtspClient->envir(); // alias
    StreamClientState &scs = frameRtspClient->scs;    // alias

    if (resultCode != 0) {
      LOGGER->error("{} Failed to get a SDP description: {}", *rtspClient,
                    resultString);
      break;
    }

    const char *sdpDescription = resultString;
    LOGGER->info("{} Got an SDP description:", *rtspClient);
    LOGGER->info("{}", sdpDescription);

    // Create a media session object from this SDP description:
    scs.session = MediaSession::createNew(env, sdpDescription);
    if (scs.session == nullptr) {
      LOGGER->error("{} Failed to create a MediaSession object from the SDP "
                    "description: {}",
                    *rtspClient, env.getResultMsg());
      break;
    } else if (!scs.session->hasSubsessions()) {
      LOGGER->warn("{} This session has no media subsessions (i.e., no \"m=\" "
                   "lines)",
                   *rtspClient);
      break;
    }

    // Then, create and set up our data source objects for the session.  We do
    // this by iterating over the session's 'subsessions', calling
    // "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command,
    // on each one. (Each 'subsession' will have its own data source.)
    scs.iter = std::make_unique<MediaSubsessionIterator>(*scs.session);
    setupNextSubsession(rtspClient);
    return;
  } while (0);

  // An unrecoverable error occurred with this stream.
  frameRtspClient->rVideoSource_.StopStream();
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP,
// change the following to True:
#define REQUEST_STREAMING_OVER_TCP False

void setupNextSubsession(RTSPClient *rtspClient) {
  auto *frameRtspClient = dynamic_cast<FrameRtspClient *>(rtspClient);
  if (!frameRtspClient) {
    return;
  }
  UsageEnvironment &env = rtspClient->envir();                   // alias
  StreamClientState &scs = ((FrameRtspClient *)rtspClient)->scs; // alias

  scs.subsession = scs.iter->next();
  if (scs.subsession) {
    if (!scs.subsession->initiate()) {
      LOGGER->error("{} Failed to initiate the \"{}\" subsession: {}",
                    *rtspClient, *scs.subsession, env.getResultMsg());
      setupNextSubsession(
          rtspClient); // give up on this subsession; go to the next one
    } else {
      const std::string muxMsg =
          scs.subsession->rtcpIsMuxed()
              ? std::format("client port {}", scs.subsession->clientPortNum())
              : std::format("client ports {}-{}",
                            scs.subsession->clientPortNum(),
                            scs.subsession->clientPortNum() + 1);
      LOGGER->info("{} Initiated the \"{}\" subsession ({})", *rtspClient,
                   *scs.subsession, muxMsg);

      // Continue setting up this subsession, by sending a RTSP "SETUP"
      // command:
      rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False,
                                   REQUEST_STREAMING_OVER_TCP);
    }
    return;
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP
  // "PLAY" command to start the streaming:
  if (scs.session->absStartTime()) {
    // Special case: The stream is indexed by 'absolute' time, so send an
    // appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY,
                                scs.session->absStartTime(),
                                scs.session->absEndTime());
  } else {
    scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
  }
}

void continueAfterSETUP(RTSPClient *rtspClient, int resultCode,
                        char *resultString) {
  auto upResultString = std::unique_ptr<char>(resultString);
  auto *frameRtspClient = dynamic_cast<FrameRtspClient *>(rtspClient);
  if (!frameRtspClient) {
    return;
  }
  do {
    UsageEnvironment &env = rtspClient->envir();   // alias
    StreamClientState &scs = frameRtspClient->scs; // alias

    if (resultCode != 0) {
      LOGGER->error("{} Failed to set up the \"{}\" subsession: {}",
                    *rtspClient, *scs.subsession, resultString);
      break;
    }

    const std::string muxMsg =
        scs.subsession->rtcpIsMuxed()
            ? std::format("client port {}", scs.subsession->clientPortNum())
            : std::format("client ports {}-{}", scs.subsession->clientPortNum(),
                          scs.subsession->clientPortNum() + 1);
    LOGGER->info("{} Initiated the \"{}\" subsession ({})", *rtspClient,
                 *scs.subsession, muxMsg);

    // Having successfully setup the subsession, create a data sink for it,
    // and call "startPlaying()" on it. (This will prepare the data sink to
    // receive data; the actual flow of data from the client won't start
    // happening until later, after we've sent a RTSP "PLAY" command.)

    if ("video"sv == scs.subsession->mediumName() &&
        "H264"sv == scs.subsession->codecName()) {
      scs.subsession->sink = FrameSetterSink::CreateNew(
          env, *scs.subsession, frameRtspClient->rVideoSource_);
      if (!scs.subsession->sink) {
        LOGGER->error(
            "{} Failed to create a data sink for the \"{}\" subsession: {}",
            *rtspClient, *scs.subsession, env.getResultMsg());
        break;
      }
    } else {
      break;
    }
    // no sink for audio channels

    LOGGER->info("{} Created a data sink for the \"{}\" subsession",
                 *rtspClient, *scs.subsession);
    scs.subsession->miscPtr =
        rtspClient; // a hack to let subsession handler functions get the
                    // "RTSPClient" from the subsession
    scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
                                       subsessionAfterPlaying, scs.subsession);
    // Also set a handler to be called if a RTCP "BYE" arrives for this
    // subsession:
    if (scs.subsession->rtcpInstance() != nullptr) {
      scs.subsession->rtcpInstance()->setByeWithReasonHandler(
          subsessionByeHandler, scs.subsession);
    }
  } while (0);

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient *rtspClient, int resultCode,
                       char *resultString) {
  Boolean success = False;
  auto upResultString = std::unique_ptr<char>(resultString);
  auto *frameRtspClient = dynamic_cast<FrameRtspClient *>(rtspClient);
  if (!frameRtspClient) {
    return;
  }

  do {
    UsageEnvironment &env = rtspClient->envir();   // alias
    StreamClientState &scs = frameRtspClient->scs; // alias

    if (resultCode != 0) {
      LOGGER->error("{} Failed to start playing session: {}", *rtspClient,
                    resultString);
      break;
    }
    LOGGER->info("{} Started playing session", *rtspClient);
    if (scs.duration > 0) {
      LOGGER->info("(for up to {} seconds)", scs.duration);
    }

    // Set up a watchdog timer to make sure the stream is receiving timely
    // updates, otherwise stop waiting and close
    scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(
        frameRtspClient->waitForFrame.count(),
        (TaskFunc *)streamWatchdogHandler, rtspClient);

    success = True;
  } while (0);

  if (!success) {
    // An unrecoverable error occurred with this stream.
    frameRtspClient->rVideoSource_.StopStream();
  }
}

// Implementation of the other event handlers:

void subsessionAfterPlaying(void *clientData) {
  if (!clientData) {
    return;
  }
  MediaSubsession *subsession = static_cast<MediaSubsession *>(clientData);
  auto *frameRtspClient = static_cast<FrameRtspClient *>(subsession->miscPtr);
  if (!frameRtspClient || !subsession) {
    return;
  }

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = nullptr;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession &session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (!subsession->sink) {
      return; // this subsession is still active
    }
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  frameRtspClient->rVideoSource_.StopStream();
}

void subsessionByeHandler(void *clientData, const char *reason) {
  MediaSubsession *subsession = (MediaSubsession *)clientData;
  RTSPClient *rtspClient = (RTSPClient *)subsession->miscPtr;

  if (reason) {
    LOGGER->warn(
        "{} Received RTCP \"BYE\" (REASON:\"{}\") on \"{}\" subsession",
        *rtspClient, reason, *subsession);
    delete[] (char *)reason;
  } else {
    LOGGER->info("{} Received RTCP \"BYE\" on \"{}\" subsession", *rtspClient,
                 *subsession);
  }

  // Now act as if the subsession had closed:
  subsessionAfterPlaying(subsession);
}

void streamWatchdogHandler(void *clientData) {
  auto *client = static_cast<FrameRtspClient *>(clientData);

  UsageEnvironment &env = client->envir(); // alias
  StreamClientState &scs = client->scs;    // alias

  const auto timeout = client->waitForFrame;
  if (std::chrono::steady_clock::now() - client->lastUpdate > timeout) {
    LOGGER->error("Timeout waiting for next frame ({}) closing stream...",
                  timeout.count());
    client->rVideoSource_.StopStream();
    return;
  }

  scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(
      client->waitForFrame.count(), (TaskFunc *)streamWatchdogHandler, client);
}

void shutdownStream(RTSPClient *rtspClient) {
  if (!rtspClient) {
    // no client to shutdown
    return;
  }
  auto *frameRtspClient = dynamic_cast<FrameRtspClient *>(rtspClient);
  StreamClientState &scs = frameRtspClient->scs; // alias

  // First, check whether any subsessions have still to be closed:
  if (scs.session != nullptr) {
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*scs.session);
    MediaSubsession *subsession;

    while ((subsession = iter.next())) {
      if (subsession->sink) {
        Medium::close(subsession->sink);
        subsession->sink = nullptr;

        if (subsession->rtcpInstance()) {
          subsession->rtcpInstance()->setByeHandler(
              nullptr, nullptr); // in case the server sends a RTCP "BYE" while
                                 // handling "TEARDOWN"
        }

        someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive) {
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the
      // stream. Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.session, nullptr);
    }
  }

  LOGGER->info("{} Closing the stream.", *rtspClient);
  Medium::close(rtspClient);
  // Note that this will also cause this stream's "StreamClientState"
  // structure to get reclaimed.
}

} // namespace video_source