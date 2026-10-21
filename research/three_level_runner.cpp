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

using Leaders =
    std::map<std::string, std::vector<std::pair<std::string, std::string>>>;

Leaders readLeaders(const std::string &Filename) {
  Leaders Result;
  std::ifstream Input(Filename);
  std::string Line;
  std::getline(Input, Line);

  while (std::getline(Input, Line)) {
    const auto Fields = split(Line);
    if (Fields.size() == 7)
      Result[Fields[1]].push_back({Fields[2], Fields[3]});
  }

  return Result;
}

std::set<std::string> makeCandidates(
    const std::vector<std::pair<std::string, std::string>> &Leaders) {
  std::set<std::string> Candidates;

  for (const auto &Pair : Leaders)
    for (const std::string &Third : research_runner::policies())
      Candidates.insert(Pair.first + "," + Pair.second + "," + Third);

  return Candidates;
}

int main(int argc, char **argv) {

  // Extend each two-level leader with every possible third policy.
  const std::string LeadersFile =
      argc > 3 ? argv[3] : "research/results/two_level_leaders.csv";

  const std::string CostLeadersFile =
      (std::filesystem::path(LeadersFile).parent_path() /
       "two_level_cost_leaders.csv")
          .string();

  const auto Settings    = research_runner::settingsWithLeaders(argc, argv, 30);
  const auto Files       = research_runner::temporaryFiles("three_level");
  const auto HitLeaders  = readLeaders(LeadersFile);
  const auto CostLeaders = std::filesystem::exists(CostLeadersFile)
                               ? readLeaders(CostLeadersFile)
                               : HitLeaders;

  std::filesystem::create_directories("research/results");
  std::ofstream Csv("research/results/three_level_results.csv");
  std::ofstream CostCsv("research/results/three_level_cost_results.csv");
  
  CostCsv << std::setprecision(17);

  Csv << "scenario,capacity_per_level,requests,level1,level2,level3,hits,ideal_"
         "hits,normalized\n";

  CostCsv << "scenario,level1_capacity,level2_capacity,level3_capacity,"
             "requests,level1,level2,level3,level1_cost,level2_cost,"
             "level3_cost,miss_cost,total_cost\n";

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

    research_runner::writeConfig(Files.Config, {"OPT", "OPT", "OPT"});
    const double IdealCost = research_runner::runCacheCost(
        Settings.Cache, Files.Config, Files.Trace, Files.Result);
    if (IdealCost < 0.0)
      return 1;

    Csv << Scenario << ',' << Settings.Capacity << ',' << Settings.Requests
        << ",OPT(3C),-,-," << IdealHits << ',' << IdealHits << ",1.000000\n";
    const int SecondCapacity =
        research_runner::CostCapacityGrowth * Settings.Capacity;
    const int ThirdCapacity =
        research_runner::CostCapacityGrowth * SecondCapacity;
    CostCsv << Scenario << ',' << Settings.Capacity << ',' << SecondCapacity
            << ',' << ThirdCapacity << ',' << Settings.Requests
            << ",OPT,OPT,OPT,1,10,100,1000," << IdealCost << '\n';

    const auto HitCandidates  = makeCandidates(HitLeaders.at(Scenario));
    const auto CostCandidates = makeCandidates(CostLeaders.at(Scenario));
    auto Candidates           = HitCandidates;
    Candidates.insert(CostCandidates.begin(), CostCandidates.end());

    for (const std::string &Candidate : Candidates) {
      const auto Parts = split(Candidate);
      research_runner::writeConfig(Files.Config,
                                   {Parts[0], Parts[1], Parts[2]});

      if (HitCandidates.count(Candidate) != 0) {
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

      if (CostCandidates.count(Candidate) != 0) {
        const double Cost = research_runner::runCacheCost(
            Settings.Cache, Files.Config, Files.Trace, Files.Result);
        if (Cost < 0.0)
          return 1;

        CostCsv << Scenario << ',' << Settings.Capacity << ',' << SecondCapacity
                << ',' << ThirdCapacity << ',' << Settings.Requests << ','
                << Parts[0] << ',' << Parts[1] << ',' << Parts[2]
                << ",1,10,100,1000," << Cost << '\n';
      }
    }
  }

  std::filesystem::remove(Files.Trace);
  std::filesystem::remove(Files.IdealTrace);
  std::filesystem::remove(Files.Config);
  std::filesystem::remove(Files.Result);

  return 0;
}
