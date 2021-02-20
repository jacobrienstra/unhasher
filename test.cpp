#include <limits>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <regex>

#include "clipp.h"
using namespace clipp;
using namespace std;

int main(int argc, char** argv)
{

  bool search3 = false;
  bool searchSubStr = false;
  string hash;

  vector<string> substrHashes;

  // auto is_hex = [](const string& arg) {
  //   return regex_match(arg, regex("(0x)?[a-fA-F0-9]{8}"));
  // };

  auto cli = (
    option("-fi", "--fitfit")
    .set(search3)
    .doc("Search 3 word table too"),
    option("-tt", "--test").set(searchSubStr) & values("stuff").set(substrHashes)
    );



  parsing_result res = parse(argc, argv, cli);
  cout << to_string(search3) << endl;

  if (res.any_error()) {
    cout << endl << make_man_page(cli, "Search") << endl;
  }

}
