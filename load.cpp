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

typedef unsigned int uint;

using namespace std;

int main(int argc, char* argv[]) {
  cout << endl << "-----------Fn Start-----------" << endl;
  cout << "LOAD" << endl;

  int opt;
  bool parts = false;
  bool full = false;
  bool hashes = false;
  bool combos2 = false;
  bool combos3 = false;
  bool newParts = true;

  set<string> usrParts;


  while ((opt = getopt(argc, argv, "pfh23n:")) != -1)
  {
    switch (opt)
    {
    case 'p':
      parts = true;
      cout << "Loading label parts" << endl;
      break;
    case 'f':
      full = true;
      cout << "Loading full labels" << endl;
      break;
    case 'h':
      hashes = true;
      cout << "Loading unknown hashes" << endl;
      break;
    case '2':
      combos2 = true;
      cout << "Loading 2 word combos..." << endl;
      break;
    case '3':
      combos3 = true;
      cout << "Loading 3 word combos..." << endl;
      break;
    case 'n':
      newParts = true;
      optind--;
      for (;optind < argc && *argv[optind] != '-'; optind++) {
        usrParts.insert(argv[optind]);
      }
      cout << "Label parts to add:";
      for (string part : usrParts) {
        cout << " " << part << ",";
      }
      cout << endl;
      break;
    case '?':
      if (optopt == 'n') {
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
    if (parts)
    {
      ifstream fvalues("corpus/values.txt");
      if (fvalues.fail()) { return 1; }
      pqxx::work wvalues(c);

      // Tags
      pqxx::stream_to stream{
        wvalues,
        "labels_parts",
        std::vector<std::string>{"text", "hash", "count", "source"}
      };
      // Main reading loop
      while (fvalues.good())
      {
        fvalues >> wordBuf >> std::dec >> count >> std::hex >> hash;
        vector<string> source = { "VALUE" };
        stream.write_values(wordBuf, (int)hash, count, source);
        loadedParts++;
        cout << "\rLoaded " << std::dec << loadedParts << " parts";
      }
      cout << "\nDone with values\n";
      loadedParts = 0;
      stream.complete();
      wvalues.commit();
      fvalues.close();


      pqxx::work wtags(c);
      ifstream ftags("corpus/tags.txt");
      if (ftags.fail()) { return 1; }
      while (ftags.good())
      {
        ftags >> wordBuf >> std::dec >> count >> std::hex >> hash;
        wtags.exec("INSERT INTO labels_parts (text, hash, count, source) VALUES (" +
          wtags.quote(wordBuf) +
          ", " +
          to_string((int)hash) +
          ", " +
          to_string(count) +
          ", '{\"TAG\"}') ON CONFLICT (text) DO UPDATE SET count = labels_parts.count + EXCLUDED.count, source = labels_parts.source || EXCLUDED.source WHERE NOT labels_parts.source && EXCLUDED.source"
        );
        loadedParts++;
        cout << "\rLoaded " << std::dec << loadedParts << " parts";
      }
      cout << "\nDone with tags\n";
      loadedParts = 0;
      wtags.commit();
      ftags.close();

      pqxx::work wids(c);
      ifstream fids("corpus/ids.txt");
      if (fids.fail()) { return 1; }
      while (fids.good())
      {
        fids >> wordBuf >> std::dec >> count >> std::hex >> hash;
        wids.exec("INSERT INTO labels_parts (text, hash, count, source) VALUES (" +
          wids.quote(wordBuf) +
          ", " +
          to_string((int)hash) +
          ", " +
          to_string(count) +
          ", '{\"ID\"}') ON CONFLICT (text) DO UPDATE SET count = labels_parts.count + EXCLUDED.count, source = labels_parts.source || EXCLUDED.source WHERE NOT labels_parts.source && EXCLUDED.source"
        );
        loadedParts++;
        cout << "\rLoaded " << std::dec << loadedParts << " parts";
      }
      cout << "\nDone with ids\n";
      loadedParts = 0;
      wids.commit();
      fids.close();
    }
    if (full)
    {
      pqxx::work wfulltags(c);
      ifstream ffulltags("corpus/fullTags.txt");
      if (ffulltags.fail()) { return 1; }
      while (ffulltags.good())
      {
        ffulltags >> wordBuf >> std::hex >> hash;
        wfulltags.exec("INSERT INTO labels_full (text, hash, source) VALUES (" +
          wfulltags.quote(wordBuf) +
          ", " +
          to_string((int)hash) +
          ", '{\"TAG\"}') ON CONFLICT (text) DO UPDATE SET source = labels_full.source || EXCLUDED.source WHERE NOT labels_full.source && EXCLUDED.source"
        );
        loadedParts++;
        cout << "\rLoaded " << std::dec << loadedParts << " full tags";
      }
      cout << "\nDone with tags\n";
      loadedParts = 0;
      wfulltags.commit();
      ffulltags.close();

      pqxx::work wfullids(c);
      ifstream ffullids("corpus/fullIds.txt");
      if (ffullids.fail()) { return 1; }
      string line;
      while (ffullids.good())
      {
        getline(ffullids, line);
        string delim = "          ";
        string tag = line.substr(0, line.find(delim));
        line.erase(0, line.find(delim) + delim.length());
        sscanf(line.c_str(), "%x", &hash);

        wfullids.exec("INSERT INTO labels_full (text, hash, source) VALUES (" +
          wfullids.quote(tag) +
          ", " +
          to_string((int)hash) +
          ", '{\"ID\"}') ON CONFLICT (text) DO UPDATE SET source = labels_full.source || EXCLUDED.source WHERE NOT labels_full.source && EXCLUDED.source"
        );
        loadedParts++;
        cout << "\rLoaded " << std::dec << loadedParts << " full ids";
      }
      cout << "\nDone with ids\n";
      loadedParts = 0;
      wfullids.commit();
      ffullids.close();
    }
    if (hashes)
    {
      pqxx::work whashes(c);
      ifstream fhashes("corpus/unTranslatedHashes.txt");
      if (fhashes.fail()) { return 1; }
      while (fhashes.good())
      {
        fhashes >> std::hex >> hash;
        whashes.exec("INSERT INTO unknown_hashes (hash) VALUES (" +
          to_string((int)hash) +
          ") ON CONFLICT (hash) DO NOTHING"
        );
        loadedParts++;
        cout << "\rLoaded " << std::dec << loadedParts << " unknown hashes";
      }
      cout << "\nDone with ids\n";
      loadedParts = 0;
      whashes.commit();
      fhashes.close();
    }
    if (combos2)
    {
      genAllCombos(2);
    }
    else if (combos3)
    {
      genAllCombos(3);
    }
    else if (newParts) {
      findNewPartCombos(usrParts, 2);
      set<pair<string, uint> > usedStrs = findNewPartCombos(usrParts, 3);
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