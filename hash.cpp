#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <iomanip>
#include <unistd.h>
#include <pqxx/pqxx>

#define HASH 5381
#define M 33

#include "hasher.h"
#include "combos.h"

using namespace std;

int main(int argc, char* argv[])
{
    cout << endl << "-----------Fn Start-----------" << endl;
    cout << "HASHER" << endl;
    int opt;
    bool loadCombos = false;

    // check for correct number of args
    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            loadCombos = true;
            cout << "Gen combos and insert mode ON" << endl;
            break;
        case '?':
            if (isprint(optopt)) {
                cout << "Unknown option -" << (char)optopt << endl;
            }
            else {
                cout << "Unknown option character \\x" << std::hex << optopt << endl;
            }
            break;
        }
    }

    string str;
    if (optind < argc) {
        str = argv[optind];
    }

    cout << "text: '" << str << "'" << endl;

    unsigned int hash = hasher(str.c_str(), HASH);
    char buf[10];
    sprintf(buf, "%08x", hash);
    string hashStr = buf;

    cout << endl << hashStr << endl << endl;

    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai_test connect_timeout=10" };

    // Check if calculated hash matches unknown hash
    pqxx::work w1(c);
    pqxx::result r1 = w1.exec("SELECT FROM unknown_hashes WHERE hash = " + to_string((int)hash));
    w1.commit();
    if (!r1.empty()) {
        cout << "Hash " << hashStr << " found!" << endl;
        // Check if already exists as a label part
        pqxx::work wparts(c);
        pqxx::result rparts = wparts.exec("SELECT source FROM labels_parts WHERE text = " + c.quote(str));
        wparts.commit();
        if (!rparts.empty()) {
            cout << " exists in parts table: " << rparts[0][0] << endl;
        }

        // Check if already exists as a full label
        pqxx::work wfull(c);
        pqxx::result rfull = wfull.exec("SELECT source FROM labels_full WHERE text = " + c.quote(str));
        wfull.commit();
        if (!rfull.empty()) {
            cout << c.quote(str) << " exists in full table: " << rfull[0][0] << endl;
        }
        else {
            cout << endl << c.quote(str) << " not in db" << endl;
            if (loadCombos) {
                cout << "2 word combos with " << c.quote(str) << "..." << endl;
                set<string> s;
                s.insert(str);
                findNewPartCombos(s, 2);

                cout << "Inserting " << c.quote(str) << " into full labels..." << endl;
                pqxx::work w3(c);
                w3.exec("INSERT INTO labels_full (text, hash, source) VALUES (" + c.quote(str) + ", " + to_string((int)hash) + ", " + "'{DISCOVERED}') ON CONFLICT (text) DO UPDATE SET source = labels_full.source || EXCLUDED.source WHERE NOT labels_full.source && EXCLUDED.source");
                w3.commit();
                //TODO: error check
                cout << c.quote(str) << " inserted" << endl;
            }
        }
    }
    else {
        cout << hashStr << " not found in db" << endl;
    }

    cout << "Done hashing" << endl;
    cout << "-----------Fn End-----------" << endl;
    return 0;
}
