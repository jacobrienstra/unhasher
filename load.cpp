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

#include "argparse.hpp"
#include "combos.h"

using namespace std;

/**
 *
 * Database functions
 *
**/
/************************************/
// Loads parts and labels into corpus
// and unknown hashes into the search corpus numWords
void loadDB(int numWords, Corpus& corp, SearchHashes& searchCorp, bool silent = true)
{
  cout << endl << "LOAD DB" << endl;
  if (!silent) cout << "Connecting to db..." << endl;
  pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };
  if (!silent) cout << "Connected" << endl;

  if (!silent) cout << "Fetching parts..." << endl;
  pqxx::work w(c);
  pqxx::result r;
  string condition = numWords == 3 ? "WHERE NOT source = '{VALUE}'" : "";
  string query = "SELECT text, hash FROM labels_parts " + condition + " ORDER BY count DESC, source ASC";
  r = w.exec(query);
  w.commit();

  if (!silent) cout << "Loading " << r.size() << " parts into corpus..." << endl;
  for (pqxx::row const& row : r) {
    corp.get<Word>().insert(WordPart(row[0].c_str(), (uint)row[1].as<int>()));
  }
  if (!silent) cout << "Corpus is " << corp.size() << " large" << endl;

  if (numWords == 2)
  {
    if (!silent) cout << "Fetching full..." << endl;
    pqxx::work w2(c);
    pqxx::result r2 = w2.exec("SELECT text, hash FROM labels_full ORDER BY source ASC");
    w2.commit();
    if (!silent) cout << "Loading " << r2.size() << " full into corpus..." << endl;
    for (auto const& row : r2) {
      corp.get<Word>().insert(WordPart(row[0].c_str(), (uint)row[1].as<int>()));
    }
    if (!silent) cout << "Corpus is " << corp.size() << " large" << endl;
  }

  if (!silent) cout << "Loading unknown hashes..." << endl;
  pqxx::work w3(c);
  pqxx::result r3 = w3.exec("SELECT hash FROM hashes");
  w3.commit();
  for (auto const& row : r3) {
    searchCorp.insert(UnkHash((uint)row[0].as<int>()));
  }
  if (!silent) cout << searchCorp.size() << " unknown hashes loaded" << endl;
  cout << "Done loading db" << endl << endl;;
  return;
}
/* END loadDB */
/**************/


int main(int argc, char* argv[]) {
  cout << endl << "-----------Fn Start-----------" << endl;
  cout << "LOAD" << endl;

  int opt;
  bool parts = false;
  bool full = false;
  bool hashes = false;
  bool newParts = true;
  vector<string> partsFiles;
  vector<string> fullFiles;
  vector<string> hashesFiles;


  while ((opt = getopt(argc, argv, "p:f:h:")) != -1)
  {
    switch (opt)
    {
    case 'p':
      parts = true;
      optind--;
      for (;optind < argc && *argv[optind] != '-'; optind++) {
        partsFiles.push_back(argv[optind]);
      }
      cout << "Loading label parts from";
      for (string file : partsFiles) {
        cout << " " << file << ",";
      }
      cout << " into db" << endl;
      break;
    case 'f':
      full = true;
      optind--;
      for (;optind < argc && *argv[optind] != '-'; optind++) {
        fullFiles.push_back(argv[optind]);
      }
      cout << "Loading full labels from";
      for (string file : fullFiles) {
        cout << " " << file << ",";
      }
      cout << " into db" << endl;
      break;
    case 'h':
      hashes = true;
      optind--;
      for (;optind < argc && *argv[optind] != '-'; optind++) {
        hashesFiles.push_back(argv[optind]);
      }
      cout << "Loading hashes from";
      for (string file : fullFiles) {
        cout << " " << file << ",";
      }
      cout << " into db" << endl;
      break;
    case '?':
      if (optopt == 'p' || optopt == 'f' || optopt == 'h') {
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
    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };
    if (parts)
    {
      cout << endl << "Load parts..." << endl;
      double count;
      uint hash;
      int loadedParts = 0;
      pqxx::work wparts(c);
      for (string& file : partsFiles) {
        string input = "";
        string source = "";
        cout << "Source tag for " << file << ":" << flush;
        cin >> input;
        if (input.compare("t") == 0 || input.compare("tag") == 0) source = 'TAG';
        else if (input.compare("i") == 0 || input.compare("id") == 0) source = 'ID';
        else if (input.compare("v") == 0 || input.compare("value") == 0) source = 'VALUE';
        else if (input.compare("d") == 0 || input.compare("discovered") == 0) source = 'DISCOVERED';
        else {
          cout << "Invalid option " << input << endl;
          return 1;
        }

        ifstream fparts(file);
        if (fparts.fail()) {
          cout << "Error loading file " << file << endl;
          return 1;
        }
        // Main reading loop
        while (fparts.good()) {
          fparts >> wordBuf >> std::dec >> count >> std::hex >> hash;
          wparts.exec("INSERT INTO labels_parts (text, hash, count, source) VALUES (" +
            wparts.quote(wordBuf) +
            ", " +
            to_string((int)hash) + ", " + to_string(count) +
            ", '{" + source + "}') ON CONFLICT (text) DO UPDATE SET source = labels_parts.source || EXCLUDED.source WHERE NOT labels_parts.source && EXCLUDED.source"
          );
          loadedParts++;
          cout << "\rLoaded " << std::dec << loadedParts << " parts";
        }
        cout << "Done with parts file " << file << endl;
        fparts.close();
        fparts.clear();
      }
      wparts.commit();
    }
    if (full)
    {
      cout << endl << "Load full..." << endl;
      double count;
      uint hash;
      int loadedParts = 0;
      pqxx::work wfull(c);
      for (string& file : fullFiles) {
        string input = "";
        string source = "";
        cout << "Source tag for " << file << ":" << flush;
        cin >> input;
        if (input.compare("t") == 0 || input.compare("tag") == 0) source = 'TAG';
        else if (input.compare("i") == 0 || input.compare("id") == 0) source = 'ID';
        else if (input.compare("d") == 0 || input.compare("discovered") == 0) source = 'DISCOVERED';
        else {
          cout << "Invalid option " << input << endl;
          return 1;
        }
        ifstream ffull(file);
        if (ffull.fail()) {
          cout << "Error loading file " << file << endl;
          return 1;
        }
        while (ffull.good())
        {
          ffull >> wordBuf >> std::hex >> hash;
          wfull.exec("INSERT INTO labels_full (text, hash, source) VALUES (" +
            wfull.quote(wordBuf) +
            ", " +
            to_string((int)hash) +
            ", '{" + source + "}') ON CONFLICT (text) DO UPDATE SET source = labels_full.source || EXCLUDED.source WHERE NOT labels_full.source && EXCLUDED.source"
          );
          loadedParts++;
          cout << "\rLoaded " << std::dec << loadedParts << " full";
        }
        cout << endl << "Done with full file " << file << endl;
        ffull.close();
        ffull.clear();
      }
      wfull.commit();
    }
    if (hashes)
    {
      cout << endl << "Load hashes..." << endl;
      double count;
      uint hash;
      int loadedParts = 0;
      pqxx::work whashes(c);
      for (string& file : hashesFiles) {
        ifstream fhashes(file);
        if (fhashes.fail()) {
          cout << "Error loading file " << file << endl;
          return 1;
        }
        while (fhashes.good())
        {
          fhashes >> std::hex >> hash;
          whashes.exec("INSERT INTO unknown_hashes (hash) VALUES (" +
            to_string((int)hash) +
            ") ON CONFLICT (hash) DO NOTHING"
          );
          loadedParts++;
          cout << "\rLoaded " << std::dec << loadedParts << " hashes";
        }
        cout << endl << "Done with hashes file " << file << endl;
        fhashes.close();
        fhashes.clear();
      }
      whashes.commit();
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


// cout << "2 word combos with " << c.quote(str) << "..." << endl;
//         set<string> s;
//         s.insert(str);
//         findNewPartCombos(s, 2);

//         cout << "Inserting " << c.quote(str) << " into full labels..." << endl;
//         pqxx::work w3(c);
//         w3.exec("INSERT INTO labels_full (text, hash, source) VALUES (" + c.quote(str) + ", " + to_string((int)hash) + ", " + "'{DISCOVERED}') ON CONFLICT (text) DO UPDATE SET source = labels_full.source || EXCLUDED.source WHERE NOT labels_full.source && EXCLUDED.source");
//         w3.commit();
//         //TODO: error check
//         cout << c.quote(str) << " inserted" << endl;