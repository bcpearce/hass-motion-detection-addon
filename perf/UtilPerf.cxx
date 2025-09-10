#include "Util/BufferOperations.h"
#include <benchmark/benchmark.h>

static void BM_FillBufferCallback(benchmark::State &state) {
  std::vector<char> buffer;
  constexpr size_t dataSize = 10 * (2 << 20); // 10MB
  std::string data(dataSize, 'x');
  for (auto _ : state) {
    util::FillBufferCallback(data.data(), 1, data.size(), &buffer);
  }
  state.SetBytesProcessed(int64_t(state.iterations()) * int64_t(dataSize));
}

BENCHMARK(BM_FillBufferCallback);

BENCHMARK_MAIN();