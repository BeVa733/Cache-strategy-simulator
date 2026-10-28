#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct Result {
  std::string Scenario;
  std::string First;
  std::string Second;
  double Value      = 0.0;
  double Reference  = 0.0;
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
  const bool LowerIsBetter = Line.find("total_cost") != std::string::npos;
  std::map<std::string, std::vector<Result>> ByScenario;
  std::map<std::string, double> References;

  while (std::getline(Input, Line)) {
    const auto Fields = split(Line);

    if (LowerIsBetter) {
      if (Fields.size() != 10)
        continue;

      if (Fields[4] == "OPT") {
        References[Fields[0]] = std::stod(Fields[9]);
        continue;
      }

      ByScenario[Fields[0]].push_back(
          {Fields[0], Fields[4], Fields[5], std::stod(Fields[9])});
    } else {
      if (Fields.size() != 8 || Fields[3] == "OPT(2C)")
        continue;

      ByScenario[Fields[0]].push_back(
          {Fields[0], Fields[3], Fields[4], std::stod(Fields[5]),
           std::stod(Fields[6]), std::stod(Fields[7])});
    }
  }

  std::ofstream Output(OutputFile);
  if (LowerIsBetter)
    Output << "rank,scenario,level1,level2,total_cost,reference_cost,"
              "normalized\n";
  else
    Output << "rank,scenario,level1,level2,hits,ideal_hits,normalized\n";

  for (auto &[Scenario, Results] : ByScenario) {
    if (LowerIsBetter) {
      for (Result &Value : Results) {
        Value.Reference  = References[Scenario];
        Value.Normalized = Value.Value / Value.Reference;
      }
    }

    std::sort(Results.begin(), Results.end(),
              [LowerIsBetter](const Result &Left, const Result &Right) {
                if (Left.Normalized != Right.Normalized)
                  return LowerIsBetter ? Left.Normalized < Right.Normalized
                                       : Left.Normalized > Right.Normalized;
                return LowerIsBetter ? Left.Value < Right.Value
                                     : Left.Value > Right.Value;
              });

    const int Count = std::min<int>(Limit, Results.size());
    for (int Index = 0; Index < Count; ++Index) {
      const Result &Value = Results[Index];

      Output << std::defaultfloat << Index + 1 << ',' << Scenario << ','
             << Value.First << ',' << Value.Second << ',' << Value.Value << ','
             << Value.Reference << ',' << std::fixed << std::setprecision(6)
             << Value.Normalized << '\n';
    }
  }
}
