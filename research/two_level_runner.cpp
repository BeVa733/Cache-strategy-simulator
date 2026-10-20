#include "research_runner_common.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>

int main(int argc, char **argv) {

  // Exhaustively evaluate all two-level policy pairs.
  const auto Settings  = research_runner::settings(argc, argv, 30);
  const auto Files     = research_runner::temporaryFiles("two_level");
  const auto Policies  = research_runner::policies();
  const auto Scenarios = research_runner::multiLevelScenarios();

  std::filesystem::create_directories("research/results");
  std::ofstream Csv("research/results/two_level_results.csv");

  Csv << "scenario,capacity_per_level,requests,level1,level2,hits,ideal_hits,"
         "normalized\n";

  for (const std::string &Scenario : Scenarios) {
    if (!research_runner::generateTrace(Settings.Generator, Scenario,
                                        Settings.Capacity, Settings.Requests,
                                        Files.Trace))
      return 1;

    if (!research_runner::makeIdealTrace(Files.Trace, Files.IdealTrace,
                                         2 * Settings.Capacity))
      return 1;

    research_runner::writeConfig(Files.Config, {"OPT"});

    const int IdealHits = research_runner::runCache(
        Settings.Cache, Files.Config, Files.IdealTrace, Files.Result);

    if (IdealHits < 0)
      return 1;

    Csv << Scenario << ',' << Settings.Capacity << ',' << Settings.Requests
        << ",OPT(2C),-," << IdealHits << ',' << IdealHits << ",1.000000\n";

    for (const std::string &First : Policies) {
      for (const std::string &Second : Policies) {
        research_runner::writeConfig(Files.Config, {First, Second});

        const int Hits = research_runner::runCache(Settings.Cache, Files.Config,
                                                   Files.Trace, Files.Result);
        if (Hits < 0)
          return 1;

        const double Normalized =
            IdealHits == 0 ? 0.0 : static_cast<double>(Hits) / IdealHits;

        Csv << Scenario << ',' << Settings.Capacity << ',' << Settings.Requests
            << ',' << First << ',' << Second << ',' << Hits << ',' << IdealHits
            << ',' << std::fixed << std::setprecision(6) << Normalized << '\n';
      }
    }
  }

  std::filesystem::remove(Files.Trace);
  std::filesystem::remove(Files.IdealTrace);
  std::filesystem::remove(Files.Config);
  std::filesystem::remove(Files.Result);
  return 0;
}
