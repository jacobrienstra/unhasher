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

#include "combos.h"
#include "load.h"
using namespace std;

/**
 *
 * Database functions
 *
**/

/****************************/
/* Clears corpus of parts */
void clearDB(Corpus& corp)
{
  corp.clear();
  cout << "Corpus cleared" << endl;
}
/* END clearDB */
/***************/

int main(int argc, char* argv[]) {
  cout << endl << "-----------Fn Start-----------" << endl;
  cout << "GEN COMBOS" << endl;

  int opt;
  bool combos2 = false;
  bool combos3 = false;
  bool isNewParts = false;
  set<string> usrParts;


  while ((opt = getopt(argc, argv, "23p:")) != -1)
  {
    switch (opt)
    {
    case '2':
      combos2 = true;
      cout << "Loading 2 word combos..." << endl;
      break;
    case '3':
      combos3 = true;
      cout << "Loading 3 word combos..." << endl;
      break;
    case 'p':
      isNewParts = true;
      optind--;
      for (;optind < argc && *argv[optind] != '-'; optind++) {
        usrParts.insert(argv[optind]);
      }
      cout << "Parts to use:";
      for (string part : usrParts) {
        cout << " " << part << ",";
      }
      cout << endl;
      break;
    case '?':
      if (optopt == 'p') {
        cout << "Option '-" << (char)optopt << "' requires an argument." << endl;
      }
      if (isprint(optopt)) {
        cout << "Unknown option '-" << (char)optopt << "'." << endl;
      }
      else {
        cout << "Unknown option character \\x" << std::hex << optopt << endl;
      }
      break;
    }
  }

  try
  {
    string wordBuf;
    double count;
    uint hash;
    int loadedParts = 0;
    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };
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
    else if (isNewParts) {
      Corpus corp;
      SearchHashes searchCorp;
      loadDB(2, corp, searchCorp);
      // TODO: allow a non loading mode/search for text
      findNewPartCombos(usrParts, 2, corp, searchCorp);
      corp.clear();
      searchCorp.clear();
      loadDB(3, corp, searchCorp);
      set<pair<string, uint> > usedStrs = findNewPartCombos(usrParts, 3, corp, searchCorp);
      /**
      * Discovered parts are included in both 2 & 3 word corpuses
      * for 2, possible a part would already be included as a fullLabel or Value
      * Therefore skipped: all possible combos made already
      * but NOT in 3, so we do run through 3 word combos
      * either way, we insert all of the parts as "discovered"
      * and the 3 word corpus will include all the new parts the 2 word corpus does, and perhaps more, so we store those
      **/
      if (usedStrs.size() != 0) {
        cout << "Inserting parts in db" << endl;
        pqxx::work wNewParts(c);
        pqxx::pipeline pNewParts(wNewParts);
        for (auto it = usedStrs.begin(); it != usedStrs.end(); it++) {
          pNewParts.insert("INSERT INTO labels_parts (text, hash, source) VALUES (" + c.quote(it->first) + ", " + to_string((int)it->second) + ", '{DISCOVERED}') ON CONFLICT (text) DO UPDATE SET source = labels_parts.source || EXCLUDED.source WHERE NOT labels_parts.source && EXCLUDED.source");
        }
        pNewParts.complete();
        wNewParts.commit();
        cout << "Parts inserted" << endl;
      }
    }
  }
  catch (std::exception const& e)
  {
    std::cout << e.what() << std::endl;
    return 1;
  }

  cout << "Done loading!" << endl;
  cout << "-----------Fn End-----------" << endl;
  return 0;
}