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
#include <pqxx/pqxx>

#include "clipp.h"
#include "combos.h"
using namespace clipp;
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
  pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai_test connect_timeout=10" };
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

  vector<string> partTagFiles;
  vector<string> partIdFiles;
  vector<string> partValueFiles;
  vector<string> fullTagFiles;
  vector<string> fullIdFiles;
  vector<string> hashesFiles;

  auto cli = (
    option("-pt").doc("Parts file with tags") & values("partTagFiles").set(partTagFiles).if_missing([] { cout << "Must provide list of files" << endl;}),
    option("-pi").doc("Parts file with IDs") & values("partIdFiles").set(partIdFiles).if_missing([] { cout << "Must provide list of files" << endl;}),
    option("-pv").doc("Part files with values") & values("partValueFiles").set(partValueFiles).if_missing([] { cout << "Must provide list of files" << endl;}),
    option("-ft").doc("Full tags files") & values("fullTagFiles").set(fullTagFiles).if_missing([] { cout << "Must provide list of files" << endl;}),
    option("-fi").doc("Full IDs files") & values("fullIdFiles").set(fullIdFiles).if_missing([] { cout << "Must provide list of files" << endl;}),
    option("-h") & values("hashesFiles").set(hashesFiles).if_missing([] { cout << "Must provide list of files" << endl;})
    );

  parsing_result res = parse(argc, argv, cli);

  if (res.any_error()) {
    cout << endl << make_man_page(cli, "Load") << endl;
  }

  vector<pair<string, string> > partFiles;
  vector<pair<string, string> > fullFiles;

  for (string const& file : partTagFiles) {
    partFiles.emplace_back(pair(file, "TAG"));
  }
  for (string const& file : partIdFiles) {
    partFiles.emplace_back(pair(file, "ID"));
  }
  for (string const& file : partValueFiles) {
    partFiles.emplace_back(pair(file, "VALUE"));
  }
  for (string const& file : fullTagFiles) {
    fullFiles.emplace_back(pair(file, "TAG"));
  }
  for (string const& file : fullIdFiles) {
    fullFiles.emplace_back(pair(file, "ID"));
  }


  try
  {
    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai_test connect_timeout=10" };

    string wordBuf;
    double count;
    uint hash;
    int loadedParts = 0;

    cout << endl << "Loading part files..." << endl;

    pqxx::work wparts(c);
    for (pair<string, string>& file : partFiles) {
      ifstream fparts(file.first);
      if (fparts.fail()) {
        cout << "Error loading file " << file.first << endl;
        return 1;
      }
      // Main reading loop
      while (fparts.good()) {
        fparts >> wordBuf >> std::dec >> count >> std::hex >> hash;
        wparts.exec("INSERT INTO labels_parts (text, hash, count, source) VALUES (" +
          wparts.quote(wordBuf) +
          ", " +
          to_string((int)hash) + ", " + to_string(count) +
          ", '{" + file.second + "}') ON CONFLICT (text) DO UPDATE SET source = labels_parts.source || EXCLUDED.source WHERE NOT labels_parts.source && EXCLUDED.source"
        );
        loadedParts++;
        cout << "\rLoaded " << std::dec << loadedParts << " parts";
      }
      cout << "Done with parts tag file " << file.first << endl;
      fparts.close();
      fparts.clear();
    }
    wparts.commit();


    cout << endl << "Loading full files..." << endl;
    loadedParts = 0;

    pqxx::work wfull(c);
    for (pair<string, string>& file : fullFiles) {
      ifstream ffull(file.first);
      if (ffull.fail()) {
        cout << "Error loading file " << file.first << endl;
        return 1;
      }
      while (ffull.good())
      {
        ffull >> wordBuf >> std::hex >> hash;
        wfull.exec("INSERT INTO labels_full (text, hash, source) VALUES (" +
          wfull.quote(wordBuf) +
          ", " +
          to_string((int)hash) +
          ", '{" + file.second + "}') ON CONFLICT (text) DO UPDATE SET source = labels_full.source || EXCLUDED.source WHERE NOT labels_full.source && EXCLUDED.source"
        );
        loadedParts++;
        cout << "\rLoaded " << std::dec << loadedParts << " full";
      }
      cout << endl << "Done with full file " << file.first << endl;
      ffull.close();
      ffull.clear();
    }
    wfull.commit();


    cout << endl << "Load hashes files..." << endl;
    loadedParts = 0;

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
        whashes.exec("INSERT INTO unknown_hashes (hash, unknown) VALUES (" +
          to_string((int)hash) +
          ", true) ON CONFLICT (hash) DO NOTHING"
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
  catch (std::exception const& e)
  {
    std::cout << e.what() << std::endl;
    return 1;
  }

  cout << "Done loading!" << endl;
  cout << "-----------Fn End-----------" << endl;
  return 0;
}