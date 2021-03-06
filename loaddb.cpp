
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <iomanip>
#include <pqxx/pqxx>

#include "loaddb.h"

using namespace std;

/**
 *
 * Database functions
 *
**/
/************************************/
// Loads parts and labels into corpus
// and unknown hashes into the search corpus numWords
void loadDB(int numWords, Corpus& corp, SearchHashes& searchCorp, bool silent)
{
  cout << endl << "LOAD DB" << endl;
  if (!silent) cout << "Connecting to db..." << endl;
  pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };
  if (!silent) cout << "Connected" << endl;

  if (!silent) cout << "Fetching parts..." << endl;
  pqxx::work w(c);
  pqxx::result r;
  string condition = numWords == 3 ? "WHERE NOT source = '{VALUE}'" : "";
  string query = "SELECT text, hash, quality FROM labels_parts " + condition + " ORDER BY count DESC, source ASC";
  r = w.exec(query);
  w.commit();

  if (!silent) cout << "Loading " << r.size() << " parts into corpus..." << endl;
  for (pqxx::row const& row : r) {
    corp.get<Word>().insert(WordPart(row[0].c_str(), (uint)row[1].as<int>(), row[2].as<int>()));
  }
  if (!silent) cout << "Corpus is " << corp.size() << " large" << endl;

  if (numWords == 2)
  {
    if (!silent) cout << "Fetching full..." << endl;
    pqxx::work w2(c);
    pqxx::result r2 = w2.exec("SELECT text, hash, quality FROM labels_full ORDER BY source ASC");
    w2.commit();
    if (!silent) cout << "Loading " << r2.size() << " full into corpus..." << endl;
    for (auto const& row : r2) {
      corp.get<Word>().insert(WordPart(row[0].c_str(), (uint)row[1].as<int>(), row[2].as<int>()));
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
  cout << "Done loading db" << endl;
  return;
}
/* END loadDB */
/**************/