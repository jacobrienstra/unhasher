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

  auto is_hex = [](const string& arg) {
    return regex_match(arg, regex("(0x)?[a-fA-F0-9]{8}"));
  };

  group cli = (
    value(is_hex, "hash")
    .set(hash)
    .doc("Hash value to search for")
    .if_missing([] { cout << endl << "No valid hash value provided" << endl; }),
    option("-3", "--search3")
    .set(search3)
    .doc("Search 3 word table too"),
    (option("-s", "--substrSearch")
      .set(searchSubStr)
      & values(is_hex, "substrHashes", substrHashes)
      .if_missing([] { cout << endl << "-s option requires valid hash list" << endl;})
      )
    .doc("Return only unhashes which are a substring of any unhash of the following hashes")
    );



  parsing_result res = parse(argc, argv, cli);
  if (res.any_error()) {
    cout << endl << make_man_page(cli, "Search") << endl;
  }

}
