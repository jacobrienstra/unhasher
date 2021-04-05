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
  int quality = 0;
  vector<string> usrPartsVec;
  vector<string> usrFullVec;
  vector<string> usrHashesVec;


  auto searchOrInsert = (
    ((required("-i", "--insert")
      .set(insert)
      .doc("Insert generated combos into database (tagged as 'DISCOVERED')")
      .if_conflicted([] { cout << "\033[31mYou can only choose one mode at a time\033[0m" << endl;})
      & (option("-q", "--quality").doc("Quality level for inserted strings")
        & value("quality").set(quality)))
      | required("-s", "--search")
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
    command("gen2").set(combos2, true).doc("Generate all 2 word combos") & (option("-s", "--search")
      .set(search)
      .doc("Match generated combos against provided hashes") & values(is_hex, "usrHashes")
      .set(usrHashesVec).if_missing([] { cout << "\033[31mMust provide list of valid hashes\033[0m" << endl; }),
      (option("-i", "--insert")
        .set(insert)
        .doc("Insert generated combos into database (tagged as 'DISCOVERED')")
        .if_conflicted([] { cout << "\033[31mYou can only choose one mode at a time\033[0m" << endl;})
        & (option("-q", "--quality").doc("Quality level for inserted strings")
          & value("quality").set(quality)))) |
    command("gen3").set(combos3, true).doc("Generate all 3 word combos")
    & (option("-s", "--search")
      .set(search)
      .doc("Match generated combos against provided hashes") & values(is_hex, "usrHashes")
      .set(usrHashesVec).if_missing([] { cout << "\033[31mMust provide list of valid hashes\033[0m" << endl; }),
      (option("-i", "--insert")
        .set(insert)
        .doc("Insert generated combos into database (tagged as 'DISCOVERED')")
        .if_conflicted([] { cout << "\033[31mYou can only choose one mode at a time\033[0m" << endl;})
        & (option("-q", "--quality").doc("Quality level for inserted strings")
          & value("quality").set(quality))))
    );

  parsing_result result = parse(argc, argv, cli);

  auto doc_label = [](const parameter& p) {
    if (!p.flags().empty()) return p.flags().front();
    if (!p.label().empty()) return p.label();
    return doc_string{ "<?>" };
  };

  cout << "args -> parameter mapping:\n";
  for (const auto& m : result) {
    cout << "#" << m.index() << " " << m.arg() << " -> ";
    auto p = m.param();
    if (p) {
      cout << doc_label(*p) << " \t";
      if (m.repeat() > 0) {
        cout << (m.bad_repeat() ? "[bad repeat " : "[repeat ")
          << m.repeat() << "]";
      }
      if (m.blocked())  cout << " [blocked]";
      if (m.conflict()) cout << " [conflict]";
      cout << '\n';
    }
    else {
      cout << " [unmapped]\n";
    }
  }

  cout << "missing parameters:\n";
  for (const auto& m : result.missing()) {
    auto p = m.param();
    if (p) {
      cout << doc_label(*p) << " \t";
      cout << " [missing after " << m.after_index() << "]\n";
    }
  }

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
      used2Strs = findNewPartCombos(*newStrings, 2, corp, *toSearch, insert, quality);

      if (parts) {
        corp.clear();
        searchCorp.clear();
        loadDB(3, corp, searchCorp);
        used3Strs = findNewPartCombos(*newStrings, 3, corp, *toSearch, insert, quality);
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
            wNew.exec("INSERT INTO labels_" + table + " (text, hash, source, quality) VALUES (" + c.quote(it->first) + ", " + to_string((int)it->second) + ", '{DISCOVERED}', " + to_string(quality) + ") ON CONFLICT (text) DO UPDATE SET source = labels_" + table + ".source || EXCLUDED.source WHERE NOT labels_" + table + ".source && EXCLUDED.source");
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


//00, 01, 02, 03, 04, 05, 06, 07, 08, 09, 0A, 0B, 0C, 0D, 0E, 0F, 0a, 0b, 0c, 0d, 0e, 0f, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 1A, 1B, 1C, 1D, 1E, 1F, 1a, 1b, 1c, 1d, 1e, 1f, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 2A, 2B, 2C, 2D, 2E, 2F, 2a, 2b, 2c, 2d, 2e, 2f, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 3A, 3B, 3C, 3D, 3E, 3F, 3a, 3b, 3c, 3d, 3e, 3f, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 4A, 4B, 4C, 4D, 4E, 4F, 4a, 4b, 4c, 4d, 4e, 4f, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 5A, 5B, 5C, 5D, 5E, 5F, 5a, 5b, 5c, 5d, 5e, 5f, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 6A, 6B, 6C, 6D, 6E, 6F, 6a, 6b, 6c, 6d, 6e, 6f, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 7A, 7B, 7C, 7D, 7E, 7F, 7a, 7b, 7c, 7d, 7e, 7f, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 8A, 8B, 8C, 8D, 8E, 8F, 8a, 8b, 8c, 8d, 8e, 8f, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 9A, 9B, 9C, 9D, 9E, 9F, 9a, 9b, 9c, 9d, 9e, 9f, A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, AB, AC, AD, AE, AF, Aa, Ab, Ac, Ad, Ae, Af, B0, B1, B2, B3, B4, B5, B6, B7, B8, B9, BA, BB, BC, BD, BE, Ba, Bb, Bc, Bd, Bf, C0, C1, C2, C3, C4, C5, C6, C7, C8, C9, CA, CB, CC, CD, CE, CF, Cb, Cc, Cd, Ce, Cf, D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, DB, DC, DD, DF, Da, Db, Dc, Dd, Df, E0, E1, E2, E3, E4, E5, E6, E7, E8, E9, EA, EB, EC, ED, EE, EF, Ea, Eb, Ec, Ed, Ee, Ef, F0, F1, F2, F3, F4, F5, F6, F7, F8, F9, FA, FC, FD, FE, FF, Fa, Fc, Fd, Fe, Ff, a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aA, aB, aC, aD, aE, aF, aa, ab, ac, ad, ae, af, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, bA, bB, bC, bD, bE, bF, ba, bb, bc, bd, be, bf, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, cA, cB, cC, cD, cE, cF, ca, cb, cc, cd, ce, cf, d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, dA, dB, dC, dD, dE, dF, da, db, dc, dd, de, df, e0, e1, e2, e3, e4, e5, e6, e7, e8, e9, eA, eB, eC, eD, eE, eF, ea, eb, ec, ed, ee, ef, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, fA, fB, fC, fD, fE, fF, fa, fb, fc, fd, fe, ff


// 00, 01, 02, 03, 04, 05, 06, 07, 08, 09, 0A, 0B, 0C, 0D, 0E, 0F, 0a, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 1A, 1B, 1C, 1D, 1E, 1F, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 2A, 2B, 2C, 2E, 2F, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 3A, 3B, 3C, 3E, 3F, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 4A, 4B, 4C, 4D, 4E, 4F, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 5A, 5B, 5C, 5D, 5E, 5F, 5e, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 6A, 6B, 6C, 6D, 6E, 6F, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 7A, 7B, 7C, 7D, 7E, 7F, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 8A, 8B, 8C, 8D, 8E, 8F, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 9A, 9B, 9C, 9D, 9E, 9F, A0, A4, A5, A6, A7, A8, A9, Aa, Ad, Ae, Af, B0, B5, B6, B7, B8, B9, Ba, Bb, Bc, C0, C4, C5, C6, C7, C8, C9, Cb, Cc, Cd, Ce, Cf, D0, D3, D4, D5, D6, D7, D8, D9, Dc, Dd, E1, E2, E4, E5, E6, E7, E8, E9, EE, Eb, Ec, Ee, F0, F6, F7, F8, Fa, Fc, Fd, Fe, Ff, a6, aA, aB, aC, aD, aE, aF, bA, bB, bC, bD, bE, bF, cA, cB, cC, cD, cE, cF, cc, d9, dA, dB, dC, dD, dE, dF, eA, eB, eC, eD, eE, eF, ee, fA, fB, fC, fD, fE, fF, fa, fc, fe