set(DEB_FILENAME
    motiondetection_${PROJECT_VERSION}_${CMAKE_SYSTEM_PROCESSOR}.deb)

add_custom_target(MotionDetectionDockerDeploy ALL
                  DEPENDS ${CMAKE_SOURCE_DIR}/docker/deploy/Dockerfile)
add_custom_command(
  TARGET MotionDetectionDockerDeploy
  POST_BUILD
  COMMAND ${CMAKE_CPACK_COMMAND} -G DEB -DCPACK_DEBIAN_FILE_NAME=${DEB_FILENAME}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/${DEB_FILENAME}
          ${CMAKE_CURRENT_BINARY_DIR}/${DEB_FILENAME}
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Dockerfile
          ${CMAKE_CURRENT_BINARY_DIR}/Dockerfile
  COMMAND
    ${DOCKER_EXECUTABLE} build -t motion-detection:${PROJECT_VERSION} -t
    motion-detection:latest --build-arg MOTION_DETECTION_DEB="${DEB_FILENAME}"
    ${CMAKE_CURRENT_BINARY_DIR}
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_dependencies(MotionDetectionDockerDeploy MotionDetection)
