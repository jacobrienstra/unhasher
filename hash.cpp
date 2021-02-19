#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <iomanip>
#include <unistd.h>
#include <pqxx/pqxx>

#include "clipp.h"
#include "hasher.h"

using namespace std;
using namespace clipp;

int main(int argc, char* argv[])
{
    cout << endl << "-----------Fn Start-----------" << endl;
    cout << "HASHER" << endl;

    bool insertAndLoad = false;
    string input;

    group cli = (
        value("text").set(input).doc("Text value to hash"),
        option("-i", "--insert").set(insertAndLoad).doc("Insert word and hash as full label if found in hash table")
        );

    parsing_result res = parse(argc, argv, cli);

    cout << "Text: '" << input << "'" << endl;

    unsigned int hash = hasher(input.c_str(), 5381);
    char buf[10];
    sprintf(buf, "%08x", hash);
    string hashStr = buf;

    cout << endl << hashStr << endl << endl;

    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };

    // Check if calculated hash matches unknown hash
    pqxx::work w1(c);
    pqxx::result r1 = w1.exec("SELECT FROM unknown_hashes WHERE hash = " + to_string((int)hash));
    // TODO: might be a known hash code too; check both hash table and parts tables
    w1.commit();
    if (r1.empty()) cout << hashStr << " not found in hash table" << endl;
    else cout << "Hash " << hashStr << " found in hash table" << endl;

    // Check if already exists as a label part
    pqxx::work wparts(c);
    pqxx::result rparts = wparts.exec("SELECT source FROM labels_parts WHERE text = " + c.quote(input));
    wparts.commit();
    if (!rparts.empty()) {
        cout << c.quote(input) << " exists in parts table: " << rparts[0][0] << endl;
    }

    // Check if already exists as a full label
    pqxx::work wfull(c);
    pqxx::result rfull = wfull.exec("SELECT source FROM labels_full WHERE text = " + c.quote(input));
    wfull.commit();
    if (!rfull.empty()) {
        cout << c.quote(input) << " exists in full table: " << rfull[0][0] << endl;
    }

    pqxx::work wgen(c);
    pqxx::result rgen = wgen.exec("SELECT hash FROM labels_gen2 WHERE text = " + c.quote(input) + " UNION SELECT hash FROM labels_gen3 WHERE text = " + c.quote(input));
    wgen.commit();
    if (!rgen.empty()) {
        cout << c.quote(input) << " exists in gen tables" << endl;
    }

    cout << "Searched full db for " + c.quote(input) << endl;
    cout << "Done hashing" << endl;
    cout << "-----------Fn End-----------" << endl;
    return 0;
}
