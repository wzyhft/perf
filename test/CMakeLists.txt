add_executable(perf_test test.cpp)
target_link_libraries(perf_test Perf)
install(TARGETS perf_test)