#include "../include/Cache.hpp"

#include <iostream>
#include <vector>

int main() {
    try {
        // Read whole list of requests.
        std::vector<int> RequestTrace;

        int Key;
        while (std::cin >> Key) {
            RequestTrace.push_back(Key);
        }

        // Send all requests to cache.
        MultiLevelCache<int, int> CacheSystem(
            "config.txt",
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
