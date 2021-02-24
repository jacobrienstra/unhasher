#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <regex>
#include <pqxx/pqxx>

#include "clipp.h"
#include "loaddb.h"

using namespace std;
using namespace clipp;


//TODO: allow for searching multiple strings or hashes


int main(int argc, char** argv)
{
    cout << endl << "-----------Fn Start-----------" << endl;
    cout << "SEARCH" << endl;

    bool search3 = true;
    bool searchSubStr = false;
    bool textSearch = false;
    string input;

    vector<string> substrHashes;

    auto is_hex = [](const string& arg) {
        return regex_match(arg, regex("(0x)?[a-fA-F0-9]{8}"));
    };

    group cli = (
        value("value")
        .set(input)
        .doc("Value to search for")
        .if_missing([] { cout << endl << "\033[31mNo valid hash value provided\033[0m" << endl; }),
        (option("-t", "--text")
            .set(textSearch)
            .doc("Search by text value instead of hash") |
            (option("-3", "--search3")
                .set(search3, false)
                .doc("Search 3 word table too"),
                (option("-s", "--substrSearch")
                    .set(searchSubStr)
                    & values(is_hex, "substrHashes", substrHashes)
                    .if_missing([] { cout << endl << "\033[31m-s option requires valid hash list\033[0m" << endl;})
                    )
                .doc("Return only unhashes which are a substring of any unhash of the following hashes")))
        );


    parsing_result res = parse(argc, argv, cli);
    if (res.any_error()) {
        cout << endl << make_man_page(cli, "Search") << endl;
        return 1;
    }
    if (!textSearch && !is_hex(input)) {
        cout << endl << "\033[31mHash mode must receive hash as first positional argument\033[0m" << endl;
        cout << endl << make_man_page(cli, "Search") << endl;
        return 1;
    }

    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };

    string searchStr = c.quote(input);
    if (textSearch) {
        cout << "TEXT mode" << endl;
        cout << "Searching for: " << searchStr << endl;
    }
    else if (searchSubStr) {
        cout << "SUBSTRING mode" << endl;
        int iHash = ((int)strtol(input.c_str(), NULL, 16));
        cout << "Searching for: " << std::hex << iHash << endl;
        cout << "As substring of: [";
        for (ulong i = 0; i < substrHashes.size(); i++) {
            cout << std::hex << ((int)strtol(substrHashes[i].c_str(), NULL, 16));
            if (i != substrHashes.size() - 1) {
                cout << ", ";
            }
        }
        cout << "]" << endl;
        searchStr = to_string(iHash);
    }
    else {
        cout << "HASH mode" << endl;
        int  iHash = ((int)strtol(input.c_str(), NULL, 16));
        cout << "Searching for: " << std::hex << iHash << endl;
        searchStr = to_string(iHash);
    }

    int results = 0;



    if (searchSubStr) {
        pqxx::work wsearch(c);
        pqxx::result rsearch = wsearch.exec("SELECT text, hash FROM \
            labels_parts WHERE hash = " + searchStr + " UNION "
            "SELECT text, hash FROM labels_full WHERE hash = " + searchStr + " UNION "
            "SELECT text, hash FROM labels_gen2 WHERE hash = " + searchStr + " UNION "
            "SELECT text, hash FROM labels_gen3 WHERE hash = " + searchStr);

        string substrList;
        for (ulong i = 0; i < substrHashes.size(); i++) {
            substrList += to_string((int)strtol(substrHashes[i].c_str(), NULL, 16));
            if (i != substrHashes.size() - 1) {
                substrList += ", ";
            }
        }
        pqxx::result rsubstr = wsearch.exec("SELECT text, hash FROM \
            labels_parts WHERE hash IN (" + substrList + ") UNION "
            "SELECT text, hash FROM labels_full WHERE hash IN (" + substrList + ") UNION "
            "SELECT text, hash FROM labels_gen2 WHERE hash IN (" + substrList + ") UNION "
            "SELECT text, hash FROM labels_gen3 WHERE hash IN (" + substrList + ")");

        cout << endl;
        for (int s = 0; s < rsearch.size(); s++) {
            for (int ss = 0; ss < rsubstr.size(); ss++) {
                if (rsubstr[ss][0].as<string>().find(rsearch[s][0].as<string>()) != string::npos) {
                    cout << rsearch[s][0] << ", " << std::hex << rsearch[s][1].as<int>() << endl;
                    cout << rsubstr[ss][0] << ", " << std::hex << rsubstr[ss][1].as<int>() << endl;
                    results++;
                }
            }
        }
        wsearch.commit();
    }
    else {
        string retField = textSearch ? "hash" : "text";
        string searchField = textSearch ? "text" : "hash";
        pqxx::work wparts(c);
        pqxx::result rparts = wparts.exec("SELECT " + retField + ", source FROM labels_parts WHERE " + searchField + " = " + searchStr);
        wparts.commit();

        if (rparts.size() != 0) {
            cout << endl << "Found match(es) in label parts table:" << endl;
            for (auto const& row : rparts) {
                results++;
                if (textSearch) {
                    cout << std::hex << row[0].as<int>() << ": " << row[1] << endl;
                }
                else {
                    cout << row[0] << endl;
                }
            }
        }

        pqxx::work wfull(c);
        pqxx::result rfull = wfull.exec("SELECT " + retField + " FROM labels_full WHERE " + searchField + " = " + searchStr);
        wfull.commit();

        if (rfull.size() != 0) {
            cout << endl << "Found match(es) in full labels table:" << endl;
            for (auto const& row : rfull) {
                results++;
                if (textSearch) {
                    cout << std::hex << row[0].as<int>() << endl;
                }
                else {
                    cout << row[0] << endl;
                }
            }
        }

        pqxx::work wgen2(c);
        pqxx::result rgen2 = wgen2.exec("SELECT " + retField + " FROM labels_gen2 WHERE " + searchField + " = " + searchStr);
        wgen2.commit();

        if (rgen2.size() != 0) {
            cout << endl << "Found match(es) in 2 word generated labels table:" << endl;
            for (auto const& row : rgen2) {
                results++;
                if (textSearch) {
                    cout << std::hex << row[0].as<int>() << endl;
                }
                else {
                    cout << row[0] << endl;
                }
            }
        }

        if (search3) {
            pqxx::work wgen3(c);
            pqxx::result rgen3 = wgen3.exec("SELECT " + retField + " FROM labels_gen3 WHERE " + searchField + " = " + searchStr + "ORDER BY text ASC");
            wgen3.commit();

            if (rgen3.size() != 0) {
                cout << endl << "Found match(es) in 3 word generated labels table:" << endl;
                for (auto const& row : rgen3) {
                    results++;
                    if (textSearch) {
                        cout << std::hex << row[0].as<int>() << endl;
                    }
                    else {
                        cout << row[0] << endl;
                    }
                }
            }
        }
    }
    cout << endl << "Total of " << results << " results found\n";
    cout << "-----------Fn End-----------" << endl;
    return 0;
}
