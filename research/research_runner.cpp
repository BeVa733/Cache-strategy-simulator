#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char **argv) {
  const std::filesystem::path RunnerDir =
      std::filesystem::path(argv[0]).parent_path();
  const std::string Cache = argc > 1 ? argv[1]
                                    : (RunnerDir / "../cache").string();
  const std::string Generator =
      argc > 2 ? argv[2] : (RunnerDir / "cache_trace_generator").string();
  const int Capacity = argc > 3 ? std::stoi(argv[3]) : 64;
  const int Requests = argc > 4 ? std::stoi(argv[4]) : 10000;
  const std::string TraceFile = "research_trace.txt";
  const std::string ConfigFile = "research_config.txt";
  const std::filesystem::path ResultsDirectory = "research/results";
  std::filesystem::create_directories(ResultsDirectory);
  const std::vector<std::string> Policies = {"LRU", "LFU", "2Q", "ARC", "LIRS", "OPT"};
  const std::vector<std::string> Scenarios = {"cycle", "hot", "scan", "phase"};
  std::ofstream Csv(ResultsDirectory / "research_results.csv");
  Csv << "scenario,capacity,requests,argument,policy,hits\n";

  for (const std::string &Scenario : Scenarios) {
    const std::string Seed = Scenario == "cycle" ? "" : "42";
    std::string Generate = Generator + " " + Scenario + " " +
                           std::to_string(Capacity) + " " +
                           std::to_string(Requests);
    if (!Seed.empty()) Generate += " " + Seed;
    Generate += " > " + TraceFile;

    if (std::system(Generate.c_str()) != 0) return 1;

    for (const std::string &Policy : Policies) {
      std::ofstream Config(ConfigFile);
      Config << "1 " << Policy << "\ncosts 1\nmiss_cost 1\n";
      Config.close();

      const std::string Run = Cache + " " + ConfigFile + " < " + TraceFile +
                              " > research_result.txt";
      if (std::system(Run.c_str()) != 0) return 1;

      std::ifstream Result("research_result.txt");
      int Hits = 0;
      Result >> Hits;
      const std::string Argument = Scenario == "cycle" ? std::to_string(Capacity + 1)
                                                         : (Scenario == "hot" ? "42" : "-");
      Csv << Scenario << ',' << Capacity << ',' << Requests << ',' << Argument
          << ',' << Policy << ',' << Hits << '\n';
      std::cout << Scenario << ',' << Policy << ',' << Hits << '\n';
    }
  }

  std::remove(TraceFile.c_str());
  std::remove(ConfigFile.c_str());
  std::remove("research_result.txt");
  return 0;
}
