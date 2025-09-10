find_package(benchmark REQUIRED)

add_executable(UtilPerf UtilPerf.cxx)
target_link_libraries(UtilPerf PRIVATE Util benchmark::benchmark
                                       benchmark::benchmark_main)
