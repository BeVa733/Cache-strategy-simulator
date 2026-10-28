#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace research_runner {

inline constexpr int CostCapacityGrowth = 2;

// Shared command-line settings for research executables.
struct Settings {
  std::string Cache;
  std::string Generator;
  int Capacity;
  int Requests;
};

// Temporary input and output files used by a single experiment.
struct TemporaryFiles {
  std::string Trace;
  std::string IdealTrace;
  std::string Config;
  std::string Result;
};

inline std::filesystem::path runnerDirectory(const char *Executable) {
  // Resolve defaults relative to the runner executable.
  return std::filesystem::path(Executable).parent_path();
}

inline std::string executablePath(int argc, char **argv, int index,
                                  const std::filesystem::path &Default) {
  return argc > index ? argv[index] : Default.string();
}

inline Settings settings(int argc, char **argv, int RequestsMultiplier) {
  const auto Directory = runnerDirectory(argv[0]);
  const int Capacity   = argc > 3 ? std::stoi(argv[3]) : 64;
  return {executablePath(argc, argv, 1, Directory / "../cache"),
          executablePath(argc, argv, 2, Directory / "cache_trace_generator"),
          Capacity,
          argc > 4 ? std::stoi(argv[4]) : RequestsMultiplier * Capacity};
}

inline Settings settingsWithLeaders(int argc, char **argv,
                                    int RequestsMultiplier) {
  const auto Directory = runnerDirectory(argv[0]);
  const int Capacity   = argc > 4 ? std::stoi(argv[4]) : 64;
  return {executablePath(argc, argv, 1, Directory / "../cache"),
          executablePath(argc, argv, 2, Directory / "cache_trace_generator"),
          Capacity,
          argc > 5 ? std::stoi(argv[5]) : RequestsMultiplier * Capacity};
}

inline TemporaryFiles temporaryFiles(const std::string &Prefix) {
  return {Prefix + "_trace.txt", Prefix + "_ideal_trace.txt",
          Prefix + "_config.txt", Prefix + "_result.txt"};
}

inline std::vector<std::string> policies() {
  // OPT is used separately as an ideal reference.
  return {"LRU", "LFU", "2Q", "ARC", "LIRS"};
}

inline std::vector<std::string> policiesWithOpt() {
  auto Policies = policies();
  Policies.push_back("OPT");
  return Policies;
}

inline std::vector<std::string> scenarios() {
  // Keep the scenario order stable across all result tables.
  return {"cycle", "hot", "scan", "phase", "random", "mixed"};
}

inline std::vector<std::string> multiLevelScenarios() {
  // Multi-level policies are compared on the combined workload.
  return {"mixed"};
}

inline std::string scenarioArgument(const std::string &Scenario, int Capacity) {
  if (Scenario == "cycle")
    return std::to_string(4 * Capacity + 1);

  if (Scenario == "phase")
    return std::to_string(Capacity);

  if (Scenario == "random")
    return std::to_string(4 * Capacity);

  if (Scenario == "hot" || Scenario == "scan" || Scenario == "mixed")
    return "42";

  return {};
}

inline bool generateTrace(const std::string &Generator,
                          const std::string &Scenario, int Capacity,
                          int Requests, const std::string &TraceFile) {

  // Run the generator and redirect its trace to a temporary file.
  std::string Command        = Generator + " " + Scenario + " " +
                               std::to_string(Capacity) + " " +
                               std::to_string(Requests);
  const std::string Argument = scenarioArgument(Scenario, Capacity);
  if (!Argument.empty())
    Command += " " + Argument;

  return std::system((Command + " > " + TraceFile).c_str()) == 0;
}

inline void writeConfig(const std::string &ConfigFile,
                        const std::vector<std::string> &Levels) {
  // Use one order of magnitude per level and a fixed miss penalty.
  std::ofstream Config(ConfigFile);
  Config << Levels.size();
  for (const std::string &Level : Levels)
    Config << ' ' << Level;
  Config << "\ncosts";
  int Cost = 1;
  for (std::size_t i = 0; i < Levels.size(); ++i) {
    Config << ' ' << Cost;
    Cost *= 10;
  }
  Config << "\nmiss_cost 1000\n";
}

inline int runCache(const std::string &Cache, const std::string &ConfigFile,
                    const std::string &TraceFile,
                    const std::string &ResultFile) {
  // Return -1 when the simulator process fails.
  const std::string Command =
      Cache + " " + ConfigFile + " < " + TraceFile + " > " + ResultFile;
  if (std::system(Command.c_str()) != 0)
    return -1;

  std::ifstream Result(ResultFile);
  int Hits = 0;
  Result >> Hits;
  return Hits;
}

inline double runCacheCost(const std::string &Cache,
                           const std::string &ConfigFile,
                           const std::string &TraceFile,
                           const std::string &ResultFile) {
  // Request the accumulated access cost from the same simulator run.
  const std::string Command = Cache + " " + ConfigFile +
                              " --cost --capacity-growth " +
                              std::to_string(CostCapacityGrowth) + " < " +
                              TraceFile + " > " + ResultFile;
  if (std::system(Command.c_str()) != 0)
    return -1.0;

  std::ifstream Result(ResultFile);
  double Cost = 0.0;
  Result >> Cost;
  return Cost;
}

inline bool makeIdealTrace(const std::string &TraceFile,
                           const std::string &IdealTraceFile, int Capacity) {
  // Reuse the trace while changing only the capacity header.
  std::ifstream Trace(TraceFile);
  std::ofstream Ideal(IdealTraceFile);
  int OldCapacity = 0;
  int Requests    = 0;
  if (!(Trace >> OldCapacity >> Requests))
    return false;

  Ideal << Capacity << ' ' << Requests;
  for (int i = 0; i < Requests; ++i) {
    int Key = 0;
    if (!(Trace >> Key))
      return false;

    Ideal << ' ' << Key;
  }
  Ideal << '\n';
  return true;
}

} // namespace research_runner
