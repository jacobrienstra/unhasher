#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <vector>
#include <limits>
#include <regex>
#include <iomanip>
#include <unistd.h>
#include <pqxx/pqxx>

#include "clipp.h"
#include "combos.h"
#include "hasher.h"
#include "load.h"
using namespace clipp;
using namespace std;

/**
 *
 * Database functions
 *
**/

auto is_hex = [](const string& arg) {
  return regex_match(arg, regex("(0x)?[a-fA-F0-9]{8}"));
};

int main(int argc, char* argv[]) {
  cout << endl << "-----------Fn Start-----------" << endl;
  cout << "GEN COMBOS" << endl;

  bool combos2 = false;
  bool combos3 = false;
  bool parts = false;
  bool full = false;
  bool search = false;
  bool insert = false;
  std::set<string> usrParts;
  std::set<string> usrFull;
  std::set<string> usrHashes;

  auto searchOrInsert = (one_of(
    option("-s", "--search")
    .set(search)
    .doc("Match generated combos against provided hashes")
    & values(is_hex, "usrHashes")
    .set(usrHashes).if_missing([] { cout << "Must provide list of valid hashes" << endl; }),
    option("-i", "--insert")
    .set(insert)
    .doc("Insert generated combos into database (tagged as 'DISCOVERED')")
    .if_conflicted([] { cout << "You can only choose one mode at a time" << endl;})
  )
    );

  auto cli = (

    option("-2", "--2combos").set(combos2).doc("Generate all 2 word combos") |
    option("-3", "--3combos").set(combos3).doc("Generate all 3 word combos") |
    (option("-p", "--newparts").set(parts).doc("Generate possible combos with strings as parts") & values("newParts").set(usrParts)
      & required(searchOrInsert)) |
    (option("-f", "--newfull").set(full).doc("Generate all combos with strings as full tags/ids").if_conflicted([] { cout << "You can only choose one type of gen at a time" << endl;}) & values("newFull").set(usrFull) & required(searchOrInsert))
    );

  parsing_result res = parse(argc, argv, cli);

  if (res.any_error()) {
    cout << endl << make_man_page(cli, "Generate Combos") << endl;
  }

  try
  {
    string wordBuf;
    double count;
    uint hash;
    int loadedParts = 0;
    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai_test connect_timeout=10" };
    if (combos2)
    {
      Corpus corp;
      SearchHashes searchCorp;
      loadDB(2, corp, searchCorp);
      genAllCombos(2, corp, searchCorp);
    }
    else if (combos3)
    {
      Corpus corp;
      SearchHashes searchCorp;
      loadDB(3, corp, searchCorp);
      genAllCombos(3, corp, searchCorp);
    }
    else if (parts || full)
    {
      std::set<string>* newStrings;
      if (parts) {
        newStrings = &usrParts;
      }
      else if (full) {
        newStrings = &usrFull;
      }
      Corpus corp;
      SearchHashes searchCorp;
      SearchHashes specCorp;
      SearchHashes* toSearch;
      std::set<pair<string, uint> > used2Strs;
      std::set<pair<string, uint> > used3Strs;

      if (search) {
        for (string const& hash : usrHashes) {
          uint uiHash = ((uint)strtol(hash.c_str(), NULL, 16));
          specCorp.insert(UnkHash(uiHash));
        }
        toSearch = &specCorp;
      }
      else {
        toSearch = &searchCorp;
      }
      loadDB(2, corp, searchCorp);
      used2Strs = findNewPartCombos(*newStrings, 2, corp, *toSearch, insert);

      if (parts) {
        corp.clear();
        searchCorp.clear();
        loadDB(3, corp, searchCorp);
        used3Strs = findNewPartCombos(*newStrings, 3, corp, *toSearch, insert);
      }
      /**
      * Discovered parts are included in both 2 & 3 word corpuses
      * for 2, possible a part would already be included as a fullLabel or Value
      * Therefore skipped: all possible combos made already
      * but NOT in 3, so we do run through 3 word combos
      * either way, we insert all of the parts as "discovered"
      * and the 3 word corpus will include all the new parts the 2 word corpus does, and perhaps more, so we store those
      **/
      if (insert) {
        std::set<pair<string, uint> >* usedStrs;
        if (parts) usedStrs = &used3Strs;
        else usedStrs = &used2Strs;
        if (usedStrs->size() == 0) {
          cout << "No supplied strings used to generate combos. Nothing to insert" << endl;
        }
        else {
          string table = parts ? "parts" : "full";
          cout << "Inserting provided " << table << " into db" << endl;
          pqxx::work wNew(c);
          for (auto it = usedStrs->begin(); it != usedStrs->end(); it++) {
            wNew.exec("INSERT INTO labels_" + table + " (text, hash, source) VALUES (" + c.quote(it->first) + ", " + to_string((int)it->second) + ", '{DISCOVERED}') ON CONFLICT (text) DO UPDATE SET source = labels_" + table + ".source || EXCLUDED.source WHERE NOT labels_" + table + ".source && EXCLUDED.source");
          }
          wNew.commit();
          cout << usedStrs->size() << " " << table << " inserted" << endl;
        }
      }
    }
  }
  catch (std::exception const& e)
  {
    std::cout << e.what() << std::endl;
    return 1;
  }

  cout << "Done generating combos!" << endl;
  cout << "-----------Fn End-----------" << endl;
  return 0;
}
