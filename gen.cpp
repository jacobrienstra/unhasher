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
#include "loaddb.h"
#include "combos.h"

using namespace clipp;
using namespace std;


// TODO: Create a watch list of unknown but needed hashes
// TODO: get PrimoCombos list of tags, generate (store ALL?)
// TODO: apply score to gen tables: based on sources of parts
// TODO: only one underscore per field
// TODO: add 0, 1 and _0, _1 at end only
// Clean up bad ones? Allow only showing certain rank?

auto is_hex = [](const string& arg) {
  return regex_match(arg, regex("(0x)?[a-fA-F0-9]{8}"));
};

int main(int argc, char** argv) {
  cout << endl << "-----------Fn Start-----------" << endl;
  cout << "GENERATE COMBOS" << endl;

  bool combos2 = false;
  bool combos3 = false;
  bool parts = false;
  bool full = false;
  bool search = false;
  bool insert = false;
  vector<string> usrPartsVec;
  vector<string> usrFullVec;
  vector<string> usrHashesVec;


  auto searchOrInsert = (
    ((required("-i", "--insert")
      .set(insert)
      .doc("Insert generated combos into database (tagged as 'DISCOVERED')")
      .if_conflicted([] { cout << "\033[31mYou can only choose one mode at a time\033[0m" << endl;})) | required("-s", "--search")
      .set(search)
      .doc("Match generated combos against provided hashes")
      & values(is_hex, "usrHashes")
      .set(usrHashesVec).if_missing([] { cout << "\033[31mMust provide list of valid hashes\033[0m" << endl; }))
    );

  auto cli = (
    command("parts")
    .set(parts, true)
    .doc("Generate possible combos with strings as parts")
    & (values("newParts").if_missing([] { cout << "\033[31mMust provide list of parts\033[0m" << endl; })
      .set(usrPartsVec)
      & searchOrInsert) |
    command("full")
    .set(full, true)
    .doc("Generate all combos with strings as full tags/ids")
    .if_conflicted([] { cout << "\033[31mYou can only choose one type of gen at a time\033[0m" << endl;})
    & (values("newFull").if_missing([] { cout << "\033[31mMust provide list of full labels\033[0m" << endl; })
      .set(usrFullVec) & searchOrInsert
      ) |
    command("c2").set(combos2, true).doc("Generate all 2 word combos") & (option("-s", "--search")
      .set(search)
      .doc("Match generated combos against provided hashes") & values(is_hex, "usrHashes")
      .set(usrHashesVec).if_missing([] { cout << "\033[31mMust provide list of valid hashes\033[0m" << endl; }), option("-i", "--insert")
      .set(insert)
      .doc("Insert generated combos into database (tagged as 'DISCOVERED')")
      .if_conflicted([] { cout << "\033[31mYou can only choose one mode at a time\033[0m" << endl;})) |
    command("c3").set(combos3, true).doc("Generate all 3 word combos") & (option("-s", "--search")
      .set(search)
      .doc("Match generated combos against provided hashes") & values(is_hex, "usrHashes")
      .set(usrHashesVec).if_missing([] { cout << "\033[31mMust provide list of valid hashes\033[0m" << endl; }), option("-i", "--insert")
      .set(insert)
      .doc("Insert generated combos into database (tagged as 'DISCOVERED')")
      .if_conflicted([] { cout << "\033[31mYou can only choose one mode at a time\033[0m" << endl;}))
    );

  parsing_result result = parse(argc, argv, cli);

  // auto doc_label = [](const parameter& p) {
  //   if (!p.flags().empty()) return p.flags().front();
  //   if (!p.label().empty()) return p.label();
  //   return doc_string{ "<?>" };
  // };

  // cout << "args -> parameter mapping:\n";
  // for (const auto& m : result) {
  //   cout << "#" << m.index() << " " << m.arg() << " -> ";
  //   auto p = m.param();
  //   if (p) {
  //     cout << doc_label(*p) << " \t";
  //     if (m.repeat() > 0) {
  //       cout << (m.bad_repeat() ? "[bad repeat " : "[repeat ")
  //         << m.repeat() << "]";
  //     }
  //     if (m.blocked())  cout << " [blocked]";
  //     if (m.conflict()) cout << " [conflict]";
  //     cout << '\n';
  //   }
  //   else {
  //     cout << " [unmapped]\n";
  //   }
  // }

  // cout << "missing parameters:\n";
  // for (const auto& m : result.missing()) {
  //   auto p = m.param();
  //   if (p) {
  //     cout << doc_label(*p) << " \t";
  //     cout << " [missing after " << m.after_index() << "]\n";
  //   }
  // }

  if (result.any_error()) {
    cout << endl << make_man_page(cli, argv[0]) << endl;
    return 1;
  }

  std::set<string> usrParts(usrPartsVec.begin(), usrPartsVec.end());
  std::set<string> usrFull(usrFullVec.begin(), usrFullVec.end());
  std::set<string> usrHashes(usrHashesVec.begin(), usrHashesVec.end());


  try
  {
    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };
    if (combos2 || combos3)
    {
      SearchHashes searchCorp;
      SearchHashes specCorp;
      SearchHashes* toSearch;
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
      Corpus corp;
      loadDB(combos2 ? 2 : 3, corp, searchCorp);
      genAllCombos(combos2 ? 2 : 3, corp, *toSearch, insert);
      if (insert && search) {
        pqxx::work wHashes(c);
        for (auto it = specCorp.get<Hash>().begin(); it != specCorp.get<Hash>().end(); it++) {
          wHashes.exec("INSERT INTO hashes (hash, unknown) VALUES (" + to_string(it->hash) + ", true) ON CONFLICT (hash) DO NOTHING");
        }
        wHashes.commit();
        cout << specCorp.size() << " hashes inserted" << endl;
      }
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
          cout << "\033[31mNo supplied strings used to generate combos. Nothing to insert\033[0m" << endl;
        }
        else {
          string table = parts ? "parts" : "full";
          cout << "Inserting provided " << table << " into db" << endl;
          pqxx::work wNew(c);
          for (auto it = usedStrs->begin(); it != usedStrs->end(); it++) {
            wNew.exec("INSERT INTO labels_" + table + " (text, hash, source, quality) VALUES (" + c.quote(it->first) + ", " + to_string((int)it->second) + ", '{DISCOVERED}', 0) ON CONFLICT (text) DO UPDATE SET source = labels_" + table + ".source || EXCLUDED.source WHERE NOT labels_" + table + ".source && EXCLUDED.source");
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