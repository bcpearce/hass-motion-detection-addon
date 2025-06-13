find_program(DOCKER_EXECUTABLE "docker")
if (DOCKER_EXECUTABLE)
  execute_process(
    COMMAND ${DOCKER_EXECUTABLE} --version
    ECHO_ERROR_VARIABLE
    COMMAND_ERROR_IS_FATAL ANY
  )
  if (DOCKER_ERROR)
    message(FATAL_ERROR "Docker executable failed ${DOCKER_ERROR}")
  endif()
  message(STATUS "Docker executable found ${DOCKER_EXECUTABLE}")
else()
  message(FATAL_ERROR "Docker executable not found")
endif()