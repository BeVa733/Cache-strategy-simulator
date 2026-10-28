#include "Cache.hpp"

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
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
    std::string ConfigFilename = "config.txt";
    bool PrintCost             = false;
    bool ConfigProvided        = false;
    std::size_t CapacityGrowth = 1;

    for (int Index = 1; Index < argc; ++Index) {
      const std::string Argument = argv[Index];
      if (Argument == "--cost") {
        PrintCost = true;
      } else if (Argument == "--capacity-growth") {
        if (++Index == argc)
          throw std::runtime_error("Expected a value after --capacity-growth");

        const int Growth = std::stoi(argv[Index]);
        if (Growth <= 0)
          throw std::runtime_error("Capacity growth must be positive");

        CapacityGrowth = static_cast<std::size_t>(Growth);
      } else if (Argument.rfind("--", 0) == 0) {
        throw std::runtime_error("Unknown option: " + Argument);
      } else if (!ConfigProvided) {
        ConfigFilename = Argument;
        ConfigProvided = true;
      } else {
        throw std::runtime_error(
            "Usage: cache [config-file] [--cost] [--capacity-growth N]");
      }
    }

    const std::size_t NumLevels         = readLevelCount(ConfigFilename);
    const std::size_t Capacity          = readNonNegativeSize("cache capacity");
    const std::size_t RequestCount      = readNonNegativeSize("request count");
    const std::vector<int> RequestTrace = readTrace(RequestCount);
    std::vector<std::size_t> Capacities;
    std::size_t LevelCapacity = Capacity;

    for (std::size_t Level = 0; Level < NumLevels; ++Level) {
      Capacities.push_back(LevelCapacity);
      if (Level + 1 == NumLevels)
        continue;

      if (LevelCapacity >
          std::numeric_limits<std::size_t>::max() / CapacityGrowth)
        throw std::runtime_error("Cache capacity overflow");

      LevelCapacity *= CapacityGrowth;
    }

    MultiLevelCache<int> CacheSystem(ConfigFilename.c_str(), Capacities,
                                     RequestTrace);
    for (int Key : RequestTrace) {
      CacheSystem.accessElement(Key);
    }

    if (PrintCost)
      std::cout << std::setprecision(17) << CacheSystem.totalCost() << '\n';
    else
      std::cout << CacheSystem.hitCount() << '\n';

    return EXIT_SUCCESS;
  } catch (const std::exception &Exception) {
    std::cerr << Exception.what() << '\n';
    return EXIT_FAILURE;
  }
}
