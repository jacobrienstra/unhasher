#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <string>
#include <map>
#include <unistd.h>
#include <pqxx/pqxx>

using namespace std;

// default dictionary

int main(int argc, char** argv)
{
    int opt;
    char* hash = NULL;
    bool search3 = false;

    while ((opt = getopt(argc, argv, "3")) != -1)
    {
        switch (opt)
        {
        case '3':
            search3 = true;
            printf("Searching 3 word databases too\n");
            break;
        }
    }

    if (optind < argc) {
        hash = argv[optind];
    }

    int iHash = (int)strtol(hash, NULL, 16);
    printf("\nHash: %x\n", iHash);

    int results = 0;

    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai_test connect_timeout=10" };
    pqxx::work wparts(c);
    pqxx::result rparts = wparts.exec("SELECT text FROM labels_parts WHERE hash = " + to_string(iHash));
    wparts.commit();

    if (rparts.size() != 0) {
        cout << "\nFound match(es) in label parts table:\n";
        for (auto const& row : rparts) {
            results++;
            cout << row[0] << endl;
        }
        cout << endl;
    }

    pqxx::work wfull(c);
    pqxx::result rfull = wfull.exec("SELECT text FROM labels_full WHERE hash = " + to_string(iHash));
    wfull.commit();

    if (rfull.size() != 0) {
        cout << "\nFound match(es) in full labels table:\n\n";
        for (auto const& row : rfull) {
            results++;
            cout << row[0] << endl;
        }
        cout << endl;
    }

    pqxx::work wgen2(c);
    pqxx::result rgen2 = wgen2.exec("SELECT text FROM labels_gen2 WHERE hash = " + to_string(iHash));
    wgen2.commit();

    if (rgen2.size() != 0) {
        cout << "\nFound match(es) in 2 word generated labels table:\n";
        for (auto const& row : rgen2) {
            results++;
            cout << row[0] << endl;
        }
        cout << endl;
    }

    if (search3) {
        pqxx::work wgen3(c);
        pqxx::result rgen3 = wgen3.exec("SELECT text FROM labels_gen3 WHERE hash = " + to_string(iHash));
        wgen3.commit();

        if (rgen3.size() != 0) {
            cout << "\nFound match(es) in 3 word generated labels table:\n";
            for (auto const& row : rgen3) {
                results++;
                cout << row[0] << endl;
            }
            cout << endl;
        }
    }

    cout << "Total of " << results << " results found\n";

    return 0;
}
