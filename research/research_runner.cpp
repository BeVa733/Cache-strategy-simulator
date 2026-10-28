#include "research_runner_common.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <vector>

int main(int argc, char **argv) {

  // Run every policy independently on every scenario.
  const auto Settings = research_runner::settings(argc, argv);
  const auto Files    = research_runner::temporaryFiles("research");

  const std::filesystem::path ResultsDirectory = "research/results";
  std::filesystem::create_directories(ResultsDirectory);
  std::ofstream Csv(ResultsDirectory / "research_results.csv");
  std::ofstream CostCsv(ResultsDirectory / "research_cost_results.csv");
  CostCsv << std::setprecision(17);

  Csv << "scenario,capacity,requests,argument,policy,hits\n";
  CostCsv << "scenario,capacity,requests,argument,policy,level_cost,miss_cost,"
             "total_cost\n";

  for (int Requests : Settings.Requests) {
    for (const std::string &Scenario : research_runner::scenarios()) {
      if (!research_runner::generateTrace(Settings.Generator, Scenario,
                                          Settings.Capacity, Requests,
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

        const double Cost = research_runner::runCacheCost(
            Settings.Cache, Files.Config, Files.Trace, Files.Result);
        if (Cost < 0.0)
          return 1;

        Csv << Scenario << ',' << Settings.Capacity << ',' << Requests << ','
            << Argument << ',' << Policy << ',' << Hits << '\n';
        CostCsv << Scenario << ',' << Settings.Capacity << ',' << Requests
                << ',' << Argument << ',' << Policy << ",1,1000," << Cost
                << '\n';
      }
    }
  }

  std::filesystem::remove(Files.Trace);
  std::filesystem::remove(Files.Config);
  std::filesystem::remove(Files.Result);
  return 0;
}
