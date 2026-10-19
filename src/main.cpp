#include "../include/Cache.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

int main() {
    try {
        // Read number of levels from config to know how many capacities to expect.
        std::ifstream ConfigFile("config.txt");
        if (!ConfigFile) {
            throw std::runtime_error("Unable to open config file: config.txt");
        }

        std::size_t NumLevels = 0;
        if (!(ConfigFile >> NumLevels)) {
            throw std::runtime_error("Incorrect config format: expected number of cache levels");
        }

        // Read level capacities from stdin.
        std::vector<std::size_t> Capacities(NumLevels);
        for (std::size_t i = 0; i < NumLevels; ++i) {
            if (!(std::cin >> Capacities[i])) {
                throw std::runtime_error("Unable to read capacity for cache level " + std::to_string(i));
            }
        }

        // Read whole list of requests.
        std::vector<int> RequestTrace;

        int Key;
        while (std::cin >> Key) {
            RequestTrace.push_back(Key);
        }

        // Send all requests to cache.
        MultiLevelCache<int, int> CacheSystem(
            "config.txt",
            Capacities,
            RequestTrace
        );

        // Process requests in the original order.
        for (int Key : RequestTrace) {
            CacheSystem.accessElement(Key);
        }

        // Display the statistics.
        std::cout << "Hit count = "
                  << CacheSystem.HitCount_ << '\n';

        std::cout << "Miss count = "
                  << CacheSystem.MissCount_ << '\n';

    } catch (const std::exception& Exception) {
        std::cout << Exception.what() << '\n';
    }

    return 0;
}