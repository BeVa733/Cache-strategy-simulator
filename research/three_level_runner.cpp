#include "research_runner_common.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

std::vector<std::string> split(const std::string &Line) {
  // Parse the simple comma-separated leader table.
  std::vector<std::string> Fields;
  std::stringstream Stream(Line);
  std::string Field;
  while (std::getline(Stream, Field, ','))
    Fields.push_back(Field);
  return Fields;
}

int main(int argc, char **argv) {

  // Extend each two-level leader with every possible third policy.
  const std::string LeadersFile =
      argc > 3 ? argv[3] : "research/results/two_level_leaders.csv";
  const auto Settings = research_runner::settingsWithLeaders(argc, argv, 30);
  const auto Files    = research_runner::temporaryFiles("three_level");

  std::map<std::string, std::vector<std::pair<std::string, std::string>>>
      Leaders;
  std::ifstream LeadersInput(LeadersFile);
  std::string Line;
  std::getline(LeadersInput, Line);

  while (std::getline(LeadersInput, Line)) {
    const auto Fields = split(Line);
    if (Fields.size() == 7)
      Leaders[Fields[1]].push_back({Fields[2], Fields[3]});
  }

  std::filesystem::create_directories("research/results");
  std::ofstream Csv("research/results/three_level_results.csv");
  Csv << "scenario,capacity_per_level,requests,level1,level2,level3,hits,ideal_"
         "hits,normalized\n";

  for (const std::string &Scenario : research_runner::multiLevelScenarios()) {
    if (!research_runner::generateTrace(Settings.Generator, Scenario,
                                        Settings.Capacity, Settings.Requests,
                                        Files.Trace))
      return 1;

    if (!research_runner::makeIdealTrace(Files.Trace, Files.IdealTrace,
                                         3 * Settings.Capacity))
      return 1;

    research_runner::writeConfig(Files.Config, {"OPT"});

    const int IdealHits = research_runner::runCache(
        Settings.Cache, Files.Config, Files.IdealTrace, Files.Result);
    if (IdealHits < 0)
      return 1;

    Csv << Scenario << ',' << Settings.Capacity << ',' << Settings.Requests
        << ",OPT(3C),-,-," << IdealHits << ',' << IdealHits << ",1.000000\n";

    std::set<std::string> Candidates;
    for (const auto &Pair : Leaders[Scenario])
      for (const std::string &Third : research_runner::policies())
        Candidates.insert(Pair.first + "," + Pair.second + "," + Third);

    for (const std::string &Candidate : Candidates) {
      const auto Parts = split(Candidate);
      research_runner::writeConfig(Files.Config,
                                   {Parts[0], Parts[1], Parts[2]});
      const int Hits = research_runner::runCache(Settings.Cache, Files.Config,
                                                 Files.Trace, Files.Result);
      if (Hits < 0)
        return 1;

      const double Normalized =
          IdealHits == 0 ? 0.0 : static_cast<double>(Hits) / IdealHits;
          
      Csv << Scenario << ',' << Settings.Capacity << ',' << Settings.Requests
          << ',' << Parts[0] << ',' << Parts[1] << ',' << Parts[2] << ','
          << Hits << ',' << IdealHits << ',' << std::fixed
          << std::setprecision(6) << Normalized << '\n';
    }
  }

  std::filesystem::remove(Files.Trace);
  std::filesystem::remove(Files.IdealTrace);
  std::filesystem::remove(Files.Config);
  std::filesystem::remove(Files.Result);
  
  return 0;
}
