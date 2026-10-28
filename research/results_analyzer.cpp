#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct Result {
  std::string First;
  std::string Second;
  int Samples          = 0;
  double NormalizedSum = 0.0;
  double Worst         = 0.0;
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

void addResult(std::map<std::string, std::map<std::string, Result>> &Results,
               const std::string &Scenario, const std::string &First,
               const std::string &Second, double Normalized,
               bool LowerIsBetter) {

  const std::string Config = First + "/" + Second;
  Result &Value            = Results[Scenario][Config];
  if (Value.Samples == 0) {
    Value.First  = First;
    Value.Second = Second;
    Value.Worst  = Normalized;
  } else if (LowerIsBetter) {
    Value.Worst = std::max(Value.Worst, Normalized);
  } else {
    Value.Worst = std::min(Value.Worst, Normalized);
  }

  ++Value.Samples;
  Value.NormalizedSum += Normalized;
}

double average(const Result &Value) {

  return Value.NormalizedSum / Value.Samples;
}

// Select policies by their average normalized result across trace lengths.
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
  std::map<std::string, std::map<std::string, Result>> ByScenario;
  std::map<std::string, double> References;

  while (std::getline(Input, Line)) {
    const auto Fields = split(Line);

    if (LowerIsBetter) {
      if (Fields.size() != 10)
        continue;

      const std::string ReferenceKey = Fields[0] + ":" + Fields[3];
      if (Fields[4] == "OPT") {
        References[ReferenceKey] = std::stod(Fields[9]);
        continue;
      }

      const auto Reference = References.find(ReferenceKey);
      if (Reference == References.end())
        continue;

      const double Normalized = std::stod(Fields[9]) / Reference->second;
      addResult(ByScenario, Fields[0], Fields[4], Fields[5], Normalized, true);
    } else {
      if (Fields.size() != 8 || Fields[3] == "OPT(2C)")
        continue;

      addResult(ByScenario, Fields[0], Fields[3], Fields[4],
                std::stod(Fields[7]), false);
    }
  }

  std::ofstream Output(OutputFile);
  Output << "rank,scenario,level1,level2,samples,average_normalized,"
            "worst_normalized\n";

  for (auto &[Scenario, Configs] : ByScenario) {
    std::vector<Result> Results;
    for (const auto &Entry : Configs)
      Results.push_back(Entry.second);

    std::sort(Results.begin(), Results.end(),
              [LowerIsBetter](const Result &Left, const Result &Right) {
                if (average(Left) != average(Right))
                  return LowerIsBetter ? average(Left) < average(Right)
                                       : average(Left) > average(Right);

                if (Left.Worst != Right.Worst)
                  return LowerIsBetter ? Left.Worst < Right.Worst
                                       : Left.Worst > Right.Worst;

                if (Left.First != Right.First)
                  return Left.First < Right.First;

                return Left.Second < Right.Second;
              });

    const int Count = std::min<int>(Limit, Results.size());
    for (int Index = 0; Index < Count; ++Index) {
      const Result &Value = Results[Index];
      Output << Index + 1 << ',' << Scenario << ',' << Value.First << ','
             << Value.Second << ',' << Value.Samples << ',' << std::fixed
             << std::setprecision(6) << average(Value) << ',' << Value.Worst
             << '\n';
    }
  }
}
