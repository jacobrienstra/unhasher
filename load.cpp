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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#include "clipp.h"
#pragma clang diagnostic pop

using namespace clipp;
using namespace std;


int main(int argc, char* argv[]) {
  cout << endl << "-----------Fn Start-----------" << endl;
  cout << "LOAD" << endl;

  bool parts = false;
  bool full = false;
  bool hashes = false;
  vector<string> partTagFiles;
  vector<string> partIdFiles;
  vector<string> partValueFiles;
  vector<string> fullTagFiles;
  vector<string> fullIdFiles;
  vector<string> hashesFiles;
  vector<string> partDiscFiles;

  auto cli = (
    (
      command("parts").set(parts) &
      (option("-t") & values("partTagFiles", partTagFiles) % "Parts file with tags",
        option("-i") & values("partIdFiles", partIdFiles) % "Parts file with IDs",
        option("-v") & values("partValueFiles", partValueFiles) % "Part files with values",
        option("-d") & values("partDiscFiles", partDiscFiles) % "Part files with discovered values"
        )
      )
    | (
      command("full").set(full) &
      (
        option("-t") & values("fullTagFiles", fullTagFiles) % "Full tags files",
        option("-i") & values("fullIdFiles", fullIdFiles) % "Full IDs files"
        )
      )
    | command("hashes").set(hashes) & values("hashesFiles", hashesFiles)
    );

  parsing_result res = parse(argc, argv, cli);

  if (res.any_error()) {
    cout << endl << make_man_page(cli, "Load") << endl;
    return 1;
  }

  vector<pair<string, string> > partFiles;
  vector<pair<string, string> > fullFiles;

  for (string const& file : partTagFiles) {
    cout << "Part file '" + file + "': TAGs" << endl;
    partFiles.emplace_back(pair(file, "TAG"));
  }
  for (string const& file : partIdFiles) {
    cout << "Part file '" + file + "': IDs" << endl;
    partFiles.emplace_back(pair(file, "ID"));
  }
  for (string const& file : partValueFiles) {
    cout << "Part file '" + file + "': VALUEs" << endl;
    partFiles.emplace_back(pair(file, "VALUE"));
  }
  for (string const& file : partDiscFiles) {
    cout << "Part file '" + file + "': DISCOVEREDs" << endl;
    partFiles.emplace_back(pair(file, "DISCOVERED"));
  }
  for (string const& file : fullTagFiles) {
    cout << "Full file '" + file + "': TAGs" << endl;
    fullFiles.emplace_back(pair(file, "TAG"));
  }
  for (string const& file : fullIdFiles) {
    cout << "Full file '" + file + "': IDs" << endl;
    fullFiles.emplace_back(pair(file, "ID"));
  }

  if (parts && partFiles.size() == 0) {
    cout << "\033[31mMust provide at least one part file\033[0m" << endl;
    cout << endl << make_man_page(cli, "Load") << endl;
    return 1;
  }
  if (full && fullFiles.size() == 0) {
    cout << "\033[31mMust provide at least one full file\033[0m" << endl;
    cout << endl << make_man_page(cli, "Load") << endl;
    return 1;
  }
  if (hashes && hashesFiles.size() == 0) {
    cout << "\033[31mMust provide at least one hash file\033[0m" << endl;
    cout << endl << make_man_page(cli, "Load") << endl;
    return 1;
  }


  try
  {
    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };

    string wordBuf;
    double count;
    uint hash;
    int loadedParts = 0;

    if (parts && partFiles.size() > 0) {
      cout << endl << "Loading part files..." << endl;

      pqxx::work wparts(c);
      for (pair<string, string>& file : partFiles) {
        ifstream fparts(file.first);
        if (fparts.fail()) {
          cout << "\033[31mError loading file " << file.first << "\033[0m" << endl;
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
    }

    if (full && fullFiles.size() > 0) {
      cout << endl << "Loading full files..." << endl;
      loadedParts = 0;

      pqxx::work wfull(c);
      for (pair<string, string>& file : fullFiles) {
        ifstream ffull(file.first);
        if (ffull.fail()) {
          cout << "\033[31mError loading file " << file.first << "\033[0m" << endl;
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
    }

    if (hashes && hashesFiles.size() > 0) {
      cout << endl << "Load hashes files..." << endl;
      loadedParts = 0;

      pqxx::work whashes(c);
      for (string& file : hashesFiles) {
        ifstream fhashes(file);
        if (fhashes.fail()) {
          cout << "\033[31mError loading file " << file << "\033[0m" << endl;
          return 1;
        }
        while (fhashes.good())
        {
          fhashes >> std::hex >> hash;
          whashes.exec("INSERT INTO hashes (hash, unknown) VALUES (" +
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