#include "threadpool.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <map>
#include <random>
#include <unordered_set>

constexpr int workerThreads = 5;
constexpr int inputSize = 3;

constexpr int maxInputSize = 8;
static_assert(inputSize <= maxInputSize); // Map every input to one of 8 functions below
void f0(int &total, int input) { total += input + 12; }
void f1(int &total, int input) { total += input * 2; }
void f2(int &total, int input) { total += input / 2; }
void f3(int &total, int input) { total += input * 5; }
void f4(int &total, int input) { total += input / 5; }
void f5(int &total, int input) { total += input - 40; }
void f6(int &total, int input) { total += input % 12; }
void f7(int &total, int input) { total += input * 12; }
using ft = void (*)(int &, int);

struct accumulator {
  std::atomic<std::int64_t> count = 0;
  std::atomic<std::int64_t> total = 0;

  static auto start() -> std::chrono::steady_clock::time_point { return std::chrono::steady_clock::now(); }
  void stop(std::chrono::steady_clock::time_point start)
  {
    auto const time = std::chrono::steady_clock::now() - start;
    count += 1;
    total += time.count();
  }

  friend auto operator<<(std::ostream &out, accumulator const &self) -> std::ostream &
  {
    auto const count = self.count.load();
    auto const total = self.total.load();
    if (count == 0) {
      return (out << "N/A");
    }

    return (out << total / count << " (" << count << ')');
  }
};

void benchMap(int &dummy, int input, accumulator &acc)
{
  static std::map<int, ft> const fields{
      {0, &f0}, {1, &f1}, {2, &f2}, {3, &f3}, {4, &f4}, {5, &f5}, {6, &f6}, {7, &f7},
  };

  auto const start = acc.start();
  auto const fun = fields.find(input);
  (*(fun->second))(dummy, input);
  acc.stop(start);
}

void benchArray(int &dummy, int input, accumulator &acc)
{
  static std::array<ft, 8> fields = {&f0, &f1, &f2, &f3, &f4, &f5, &f6, &f7};

  auto const start = acc.start();
  auto const fun = fields[input];
  (*fun)(dummy, input);
  acc.stop(start);
}

void benchSwitch(int &dummy, int input, accumulator &acc)
{
  auto const start = acc.start();
  switch (input) {
  case 0:
    f0(dummy, input);
    break;
  case 1:
    f1(dummy, input);
    break;
  case 2:
    f2(dummy, input);
    break;
  case 3:
    f3(dummy, input);
    break;
  case 4:
    f4(dummy, input);
    break;
  case 5:
    f5(dummy, input);
    break;
  case 6:
    f6(dummy, input);
    break;
  case 7:
    f7(dummy, input);
    break;
  default:
    std::unreachable();
  }
  acc.stop(start);
}

int main()
{
  std::atomic<int> counter = 0;
  std::atomic<std::size_t> dummy = 0;
  accumulator accMap = {};
  accumulator accArray = {};
  accumulator accSwitch = {};

  // return 0;

  // Used by pseudo-realistic background task
  std::random_device dev;

  {
    threadpool pool(workerThreads);

    for (int j = 1; j < 100'000; ++j) {
      pool.submit([&] {
        std::ranlux48 rnd;
        rnd.seed(dev());
        std::uniform_int_distribution<int> dist(0, 1000);
        int local = 0;

        auto const localc = (counter += 1);
        if ((localc % 100) == 0) {
          // Actual benchmarks
          if ((localc % 300) == 0) {
            benchMap(local, dist(rnd) % inputSize, accMap);
          } else if (((localc + 100) % 300) == 0) {
            benchArray(local, dist(rnd) % inputSize, accArray);
          } else {
            benchSwitch(local, dist(rnd) % inputSize, accSwitch);
          }

          dummy = local;
          return;
        }

        // Pseudo-realistic background task
        std::unordered_set<int> set;
        while (true) {
          auto const dummy = dist(rnd);
          if (dummy < 5) {
            break;
          }
          set.insert(dummy);
        }
        if (set.empty()) {
          return;
        }

        int total = 0;
        for (auto const element : set) {
          total += element;
        }

        dummy += (total / set.size());
      });
    }

    pool.stop();
  }

  std::cout << "map: " << accMap << '\n' << "array: " << accArray << '\n' << "switch: " << accSwitch << std::endl;
  return (((int)dummy * 10) % 5);
}
