#include "research_runner_common.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char **argv) {

  // Run every policy independently on every scenario.
  const auto Settings = research_runner::settings(argc, argv, 30);
  const auto Files    = research_runner::temporaryFiles("research");

  const std::filesystem::path ResultsDirectory = "research/results";
  std::filesystem::create_directories(ResultsDirectory);
  std::ofstream Csv(ResultsDirectory / "research_results.csv");

  Csv << "scenario,capacity,requests,argument,policy,hits\n";

  for (const std::string &Scenario : research_runner::scenarios()) {
    if (!research_runner::generateTrace(Settings.Generator, Scenario,
                                        Settings.Capacity, Settings.Requests,
                                        Files.Trace))
      return 1;

    const std::string Argument =
        research_runner::scenarioArgument(Scenario, Settings.Capacity);

    for (const std::string &Policy : research_runner::policiesWithOpt()) {
      research_runner::writeConfig(Files.Config, {Policy});
      const int Hits = research_runner::runCache(Settings.Cache, Files.Config,
                                                 Files.Trace, Files.Result);

      if (Hits < 0)
        return 1;

      Csv << Scenario << ',' << Settings.Capacity << ',' << Settings.Requests
          << ',' << Argument << ',' << Policy << ',' << Hits << '\n';
    }
  }

  std::filesystem::remove(Files.Trace);
  std::filesystem::remove(Files.Config);
  std::filesystem::remove(Files.Result);
  return 0;
}
