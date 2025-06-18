FROM ubuntu:24.04

ARG MOTION_DETECTION_DEB

RUN test -n "$MOTION_DETECTION_DEB" || (echo "MOTION_DETECTION_DEB  not set" && false)

RUN apt-get update
RUN apt-get install apt-utils ca-certificates -y
ADD ${MOTION_DETECTION_DEB} /motiondetection.deb
RUN apt-get install -y /motiondetection.deb

ENTRYPOINT ["MotionDetection"]

