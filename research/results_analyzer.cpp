#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct Result {
  std::string Scenario;
  std::string First;
  std::string Second;
  int Hits          = 0;
  int IdealHits     = 0;
  double Normalized = 0.0;
};

std::vector<std::string> split(const std::string &Line) {
  // Parse one row of the two-level results table.
  std::vector<std::string> Fields;
  std::stringstream Stream(Line);
  std::string Field;

  while (std::getline(Stream, Field, ','))
    Fields.push_back(Field);

  return Fields;
}

// Select the strongest configurations for each scenario.
int main(int argc, char **argv) {

  const std::string InputFile =
      argc > 1 ? argv[1] : "research/results/two_level_results.csv";

  const std::string OutputFile =
      argc > 2 ? argv[2] : "research/results/two_level_leaders.csv";

  const int Limit = argc > 3 ? std::stoi(argv[3]) : 3;

  std::ifstream Input(InputFile);
  std::string Line;
  std::getline(Input, Line);
  std::map<std::string, std::vector<Result>> ByScenario;

  while (std::getline(Input, Line)) {
    const auto Fields = split(Line);

    if (Fields.size() != 8 || Fields[3] == "OPT(2C)")
      continue;

    ByScenario[Fields[0]].push_back({Fields[0], Fields[3], Fields[4],
                                     std::stoi(Fields[5]), std::stoi(Fields[6]),
                                     std::stod(Fields[7])});
  }

  std::ofstream Output(OutputFile);
  Output << "rank,scenario,level1,level2,hits,ideal_hits,normalized\n";

  for (auto &[Scenario, Results] : ByScenario) {
    std::sort(Results.begin(), Results.end(),
              [](const Result &Left, const Result &Right) {
                if (Left.Normalized != Right.Normalized)
                  return Left.Normalized > Right.Normalized;
                return Left.Hits > Right.Hits;
              });

    const int Count = std::min<int>(Limit, Results.size());
    for (int Index = 0; Index < Count; ++Index) {
      const Result &Value = Results[Index];
      
      Output << Index + 1 << ',' << Scenario << ',' << Value.First << ','
             << Value.Second << ',' << Value.Hits << ',' << Value.IdealHits
             << ',' << std::fixed << std::setprecision(6) << Value.Normalized
             << '\n';
    }
  }
}
