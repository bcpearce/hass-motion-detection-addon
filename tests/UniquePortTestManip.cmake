if(DEFINED ${UNIT_TEST_PROJECT}_TESTS)
  foreach(UNIT_TEST IN LISTS ${UNIT_TEST_PROJECT}_TESTS)
    math(EXPR TEST_PORT "${SIM_SERVER_PORT_START} + ${i}")
    set_tests_properties(
      ${UNIT_TEST}
      PROPERTIES ENVIRONMENT "SIM_SERVER_PORT=${TEST_PORT};$(ENV{ENVIRONMENT})")
    message(
      STATUS "Assigned port ${TEST_PORT} to ${UNIT_TEST} for parallel ctest")
    math(EXPR i "${i} + 1")
  endforeach()
endif()
