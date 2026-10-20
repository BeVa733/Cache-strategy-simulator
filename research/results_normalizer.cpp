#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

std::vector<std::string> split(const std::string &Line) {
  std::vector<std::string> Fields;
  std::stringstream Stream(Line);
  std::string Field;
  while (std::getline(Stream, Field, ','))
    Fields.push_back(Field);
  return Fields;
}

int main(int argc, char **argv) {
  const std::string SingleFile =
      argc > 1 ? argv[1] : "research/results/research_results.csv";
  const std::string TwoLevelFile =
      argc > 2 ? argv[2] : "research/results/two_level_results.csv";
  const std::string OutputFile =
      argc > 3 ? argv[3] : "research/results/normalized_results.csv";

  std::map<std::string, int> SingleLevelIdeal;
  std::vector<std::vector<std::string>> SingleLevelRows;
  std::ifstream Single(SingleFile);
  std::string Line;
  std::getline(Single, Line);
  while (std::getline(Single, Line)) {
    const auto Fields = split(Line);
    if (Fields.size() != 6)
      continue;
    const std::string Key =
        Fields[0] + ":" + Fields[1] + ":" + Fields[2] + ":" + Fields[3];
    const int Hits = std::stoi(Fields[5]);
    if (Fields[4] == "OPT")
      SingleLevelIdeal[Key] = Hits;
    SingleLevelRows.push_back(Fields);
  }

  std::ofstream Output(OutputFile);
  Output << "levels,scenario,capacity_total,requests,argument,config,hits,"
            "ideal_hits,normalized\n";

  for (const auto &Fields : SingleLevelRows) {
    const std::string Key =
        Fields[0] + ":" + Fields[1] + ":" + Fields[2] + ":" + Fields[3];
    const int IdealHits = SingleLevelIdeal[Key];
    const int Hits      = std::stoi(Fields[5]);
    const double Normalized =
        IdealHits == 0 ? 0.0 : static_cast<double>(Hits) / IdealHits;
    Output << "1," << Fields[0] << ',' << Fields[1] << ',' << Fields[2] << ','
           << Fields[3] << ',' << Fields[4] << ',' << Hits << ',' << IdealHits
           << ',' << std::fixed << std::setprecision(6) << Normalized << '\n';
  }

  std::ifstream TwoLevel(TwoLevelFile);
  std::getline(TwoLevel, Line);
  while (std::getline(TwoLevel, Line)) {
    const auto Fields = split(Line);
    if (Fields.size() != 8)
      continue;
    const int Capacity  = std::stoi(Fields[1]);
    const int Hits      = std::stoi(Fields[5]);
    const int IdealHits = std::stoi(Fields[6]);
    const double Normalized =
        IdealHits == 0 ? 0.0 : static_cast<double>(Hits) / IdealHits;
    Output << "2," << Fields[0] << ',' << 2 * Capacity << ',' << Fields[2]
           << ",-," << Fields[3] << '/' << Fields[4] << ',' << Hits << ','
           << IdealHits << ',' << std::fixed << std::setprecision(6)
           << Normalized << '\n';
  }
  return 0;
}
