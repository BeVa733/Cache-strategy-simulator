#include "../include/Cache.hpp"

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::size_t readLevelCount(const std::string &ConfigFilename) {
  std::ifstream ConfigFile(ConfigFilename);
  std::size_t NumLevels = 0;
  if (!ConfigFile || !(ConfigFile >> NumLevels) || NumLevels == 0) {
    throw std::runtime_error(
        "Unable to read a positive cache level count from config: " +
        ConfigFilename);
  }
  return NumLevels;
}

std::size_t readNonNegativeSize(const char *Name) {
  int Value = 0;
  if (!(std::cin >> Value) || Value < 0) {
    throw std::runtime_error(std::string("Expected a non-negative ") + Name);
  }
  return static_cast<std::size_t>(Value);
}

std::vector<int> readTrace(std::size_t Count) {
  std::vector<int> Trace;
  Trace.reserve(Count);

  for (std::size_t Index = 0; Index < Count; ++Index) {
    int Key = 0;
    if (!(std::cin >> Key)) {
      throw std::runtime_error("Expected key " + std::to_string(Index + 1) +
                               " of " + std::to_string(Count));
    }
    Trace.push_back(Key);
  }

  std::cin >> std::ws;
  if (!std::cin.eof()) {
    throw std::runtime_error("Unexpected data after the request trace");
  }

  return Trace;
}

} // namespace

int main(int argc, char **argv) {
  try {
    if (argc > 2) {
      throw std::runtime_error("Usage: cache [config-file]");
    }

    const std::string ConfigFilename    = argc == 2 ? argv[1] : "config.txt";
    const std::size_t NumLevels         = readLevelCount(ConfigFilename);
    const std::size_t Capacity          = readNonNegativeSize("cache capacity");
    const std::size_t RequestCount      = readNonNegativeSize("request count");
    const std::vector<int> RequestTrace = readTrace(RequestCount);
    const std::vector<std::size_t> Capacities(NumLevels, Capacity);

    MultiLevelCache<int> CacheSystem(ConfigFilename.c_str(), Capacities,
                                     RequestTrace);
    for (int Key : RequestTrace) {
      CacheSystem.accessElement(Key);
    }

    std::cout << CacheSystem.hitCount() << '\n';
    return EXIT_SUCCESS;
  } catch (const std::exception &Exception) {
    std::cerr << Exception.what() << '\n';
    return EXIT_FAILURE;
  }
}
