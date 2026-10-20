#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
  const int Requests = argc > 4 ? std::stoi(argv[4]) : 20 * Capacity;
  const std::string TraceFile = "two_level_trace.txt";
  const std::string IdealTraceFile = "two_level_ideal_trace.txt";
  const std::string ConfigFile = "two_level_config.txt";
  const std::string ResultFile = "two_level_result.txt";
  const std::vector<std::string> Policies = {"LRU", "LFU", "2Q", "ARC", "LIRS"};
  const std::vector<std::string> Scenarios = {"cycle", "hot", "scan", "phase", "random"};

  std::filesystem::create_directories("research/results");
  std::ofstream Csv("research/results/two_level_results.csv");
  Csv << "scenario,capacity_per_level,requests,level1,level2,hits,ideal_hits,normalized\n";

  for (const std::string &Scenario : Scenarios) {
    std::string Generate = Generator + " " + Scenario + " " +
                           std::to_string(Capacity) + " " +
                           std::to_string(Requests);
    if (Scenario == "hot" || Scenario == "scan") Generate += " 42";
    if (Scenario == "cycle") Generate += " " + std::to_string(4 * Capacity + 1);
    if (Scenario == "phase" || Scenario == "random")
      Generate += " " + std::to_string(4 * Capacity);
    Generate += " > " + TraceFile;
    if (std::system(Generate.c_str()) != 0) return 1;

    std::ifstream Trace(TraceFile);
    std::ofstream IdealTrace(IdealTraceFile);
    int TraceCapacity = 0;
    int TraceRequests = 0;
    Trace >> TraceCapacity >> TraceRequests;
    IdealTrace << 2 * Capacity << ' ' << TraceRequests;
    for (int i = 0; i < TraceRequests; ++i) {
      int Key = 0;
      Trace >> Key;
      IdealTrace << ' ' << Key;
    }
    IdealTrace << '\n';
    IdealTrace.close();

    std::ofstream IdealConfig(ConfigFile);
    IdealConfig << "1 OPT\ncosts 1\nmiss_cost 1\n";
    IdealConfig.close();
    const std::string IdealRun = Cache + " " + ConfigFile + " < " +
                                  IdealTraceFile + " > " + ResultFile;
    if (std::system(IdealRun.c_str()) != 0) return 1;
    std::ifstream IdealResult(ResultFile);
    int IdealHits = 0;
    IdealResult >> IdealHits;
    Csv << Scenario << ',' << Capacity << ',' << Requests
        << ",OPT(2C),-," << IdealHits << ',' << IdealHits << ",1.000000\n";

    for (const std::string &First : Policies) {
      for (const std::string &Second : Policies) {
        std::ofstream Config(ConfigFile);
        Config << "2 " << First << ' ' << Second << "\ncosts 1 1\nmiss_cost 1\n";
        Config.close();

        const std::string Run = Cache + " " + ConfigFile + " < " + TraceFile +
                                " > " + ResultFile;
        if (std::system(Run.c_str()) != 0) return 1;
        std::ifstream Result(ResultFile);
        int Hits = 0;
        Result >> Hits;
        const double Normalized = IdealHits == 0
                                       ? 0.0
                                       : static_cast<double>(Hits) / IdealHits;
        Csv << Scenario << ',' << Capacity << ',' << Requests << ',' << First
            << ',' << Second << ',' << Hits << ',' << IdealHits << ','
            << std::fixed << std::setprecision(6) << Normalized << '\n';
      }
    }
  }

  std::filesystem::remove(TraceFile);
  std::filesystem::remove(IdealTraceFile);
  std::filesystem::remove(ConfigFile);
  std::filesystem::remove(ResultFile);
}
