#include <algorithm>
#include <iostream>
#include <random>
#include <string>

int main(int argc, char **argv) {
  if (argc < 4) {
    std::cerr << "usage: " << argv[0]
              << " <cycle|hot|scan|phase|random> <capacity> <requests> [argument]\n";
    return 1;
  }

  const std::string scenario = argv[1];
  const int capacity         = std::stoi(argv[2]);
  const int requests         = std::stoi(argv[3]);
  const int argument         = argc > 4 ? std::stoi(argv[4]) : 42;

  if (capacity <= 0 || requests < 0) {
    std::cerr
        << "capacity must be positive and requests must be non-negative\n";
    return 1;
  }

  // The prefix follows the input format expected by cache.
  std::cout << capacity << ' ' << requests;

  // Repeat the working set in a cycle.
  if (scenario == "cycle") {

    const int workingSet = argc > 4 ? argument : capacity + 1;
    if (workingSet <= 0)
      return 1;
    for (int i = 0; i < requests; ++i)
      std::cout << ' ' << i % workingSet;

    // Send 80% of requests to hot keys and 20% to cold keys.
  } else if (scenario == "hot") {

    const int hotSize = std::max(1, capacity / 5);
    std::mt19937 random(argument);
    std::uniform_int_distribution<int> percent(0, 99);
    std::uniform_int_distribution<int> hot(0, hotSize - 1);
    std::uniform_int_distribution<int> cold(
        hotSize, std::max(hotSize + 1, capacity * 10) - 1);
    for (int i = 0; i < requests; ++i)
      std::cout << ' ' << (percent(random) < 80 ? hot(random) : cold(random));

    // Interleave hot keys with a sequential stream of new keys.
  } else if (scenario == "scan") {

    const int hotSize = std::max(1, capacity / 4);
    for (int i = 0; i < requests; ++i) {
      const int position = i % (2 * hotSize + capacity);
      std::cout << ' '
                << (position < 2 * hotSize ? position % hotSize : 1000000 + i);
    }

    // Alternate between two disjoint working sets.
  } else if (scenario == "phase") {

    const int workingSet = argc > 4 ? argument : capacity;
    if (workingSet <= 0)
      return 1;
    for (int i = 0; i < requests; ++i) {
      const int phase = (i / std::max(1, 4 * capacity)) % 2;
      std::cout << ' ' << phase * workingSet + i % workingSet;
    }

  } else if (scenario == "random") {
    // Choose every request uniformly from a fixed working set.
    const int workingSet = argc > 4 ? argument : 4 * capacity;
    if (workingSet <= 0)
      return 1;
    std::mt19937 random(42);
    std::uniform_int_distribution<int> key(0, workingSet - 1);
    for (int i = 0; i < requests; ++i)
      std::cout << ' ' << key(random);

  } else {
    std::cerr << "unknown scenario\n";
    return 1;
  }

  std::cout << '\n';
  return 0;
}
