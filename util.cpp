#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <regex>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#include "clipp.h"
#pragma clang diagnostic pop
#include "types.h"
#include "hasher.h"

using namespace std;
using namespace clipp;

void fill(uint hash, string text, string finalpart, uint goalhash, int level)
{
  if (level == 0) {
    uint finalhash = hasher(finalpart.c_str(), hash);
    if (finalhash == goalhash) {
      cout << text << finalpart << " " << hash << endl;
    }
    return;
  }
  for (char i = 32; i < 127; i++) {
    string part;
    part = i;
    string newText;
    newText = text;
    newText += part;
    uint nexthash = hasher(part.c_str(), hash);
    fill(nexthash, newText, finalpart, goalhash, level - 1);
  }
  return;
}


int main(int argc, char** argv)
{
  cout << endl << "-----------Fn Start-----------" << endl;
  cout << "SELECT TEST" << endl;

  string part1;
  string part2;
  string usrHash;
  int slots;

  auto is_hex = [](const string& arg) {
    return regex_match(arg, regex("(0x)?[a-fA-F0-9]{8}"));
  };

  group cli = (
    (command("fill") &
      value("part1", part1) % "First part"
      & value("part2", part2) % "Second Part"
      & number("slots", slots) % "Number of slots"
      & value("Goal Hash", usrHash) % "Goal Hash"
      )
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
  cout << "Searching for: " << part1;
  for (int i = 0; i < slots; i++) {
    cout << "_";
  }
  cout << part2 << " = " << std::hex << goalHash << endl;

  uint startPoint = hasher(part1.c_str());

  fill(startPoint, part1, part2, goalHash, slots);

  cout << "-----------Fn End-----------" << endl;
  return 0;
}

