#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <regex>

#include "clipp.h"
#include "types.h"
#include "hasher.h"

using namespace std;
using namespace clipp;


int main(int argc, char** argv)
{
  cout << endl << "-----------Fn Start-----------" << endl;
  cout << "SELECT TEST" << endl;

  string part1;
  string part2;
  string usrHash;

  auto is_hex = [](const string& arg) {
    return regex_match(arg, regex("(0x)?[a-fA-F0-9]{8}"));
  };

  group cli = (
    value("part1")
    .set(part1)
    .doc("First part")
    .if_missing([] { cout << endl << "\033[31mNo valid first value provided\033[0m" << endl; })
    & value("part2").set(part2).doc("Second Part").if_missing([] { cout << endl << "\033[31mNo valid second value provided\033[0m" << endl; })
    & value("Goal Hash").set(usrHash).doc("Goal Hash").if_missing([] { cout << endl << "\033[31mNo valid hash value provided\033[0m" << endl; })
    );


  parsing_result res = parse(argc, argv, cli);
  if (res.any_error()) {
    cout << endl << make_man_page(cli, "Selector") << endl;
    return 1;
  }
  if (!is_hex(usrHash)) {
    cout << endl << "\033[31mGoal Hash invalid\033[0m" << endl;
    cout << endl << make_man_page(cli, "Selector") << endl;
    return 1;
  }

  uint goalHash = ((uint)strtol(usrHash.c_str(), NULL, 16));
  cout << "Searching for: " << part1 << "__" << part2 << " = " << std::hex << goalHash << endl;

  uint startPoint = hasher(part1.c_str());

  for (char i = 32; i < 127; i++) {
    for (char j = 32; j < 127; j++) {
      char midpart[3] = { i, j, 0 };
      uint firsthash = hasher(midpart, startPoint);
      uint finalhash = hasher(part2.c_str(), firsthash);
      if (finalhash == goalHash) {
        cout << part1 << i << j << part2 << " " << finalhash << endl;
      }
    }
  }

  cout << "-----------Fn End-----------" << endl;
  return 0;
}
