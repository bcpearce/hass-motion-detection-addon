FROM ubuntu:24.04

ARG MOTION_DETECTION_DEB

RUN apt-get update
RUN apt-get install apt-utils ca-certificates -y
COPY ${MOTION_DETECTION_DEB} /motiondetection.deb
RUN apt-get install -y /motiondetection.deb

ENTRYPOINT ["MotionDetection"]

