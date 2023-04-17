#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <regex>
#include <vector>
#include <set>
#include <map>
#include <pqxx/pqxx>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#include "clipp.h"
#pragma clang diagnostic pop
#include "loaddb.h"

using namespace std;
using namespace clipp;


//TODO: allow for searching multiple strings or hashes

//TODO: edit distance < threshold between 2 hashes

int min(int x, int y, int z) { return min(min(x, y), z); }

string iterStr(vector<string>& iter) {
    string ret = "[";
    for (auto it = iter.begin(); it != iter.end(); it++) {
        if (it != iter.begin()) {
            ret += ", ";
        }
        ret += it->c_str();
    }
    ret += "]";
    return ret;
}

int editDist(string str1, string str2)
{
    int m = str1.length();
    int n = str2.length();
    // Create a table to store results of subproblems
    int dp[m + 1][n + 1];

    // Fill d[][] in bottom up manner
    for (int i = 0; i <= m; i++) {
        for (int j = 0; j <= n; j++) {
            // If first string is empty, only option is to
            // insert all characters of second string
            if (i == 0)
                dp[i][j] = j; // Min. operations = j

            // If second string is empty, only option is to
            // remove all characters of second string
            else if (j == 0)
                dp[i][j] = i; // Min. operations = i

            // If last characters are same, ignore last char
            // and recur for remaining string
            else if (str1[i - 1] == str2[j - 1])
                dp[i][j] = dp[i - 1][j - 1];

            // If the last character is different, consider
            // all possibilities and find the minimum
            else
                dp[i][j]
                = 1
                + min(dp[i][j - 1], // Insert
                    dp[i - 1][j], // Remove
                    dp[i - 1][j - 1]); // Replace
        }
    }
    return dp[m][n];
}

vector<map<string, string>> searchForCommon(vector<map<string, string>> results, std::set<string> newList, string newListHash, int editDistance) {
    for (auto kt = newList.begin(); kt != newList.end(); kt++) {
        // kt = newList string el
        bool added = false;
        for (auto it = results.begin(); it != results.end(); it++) {
            // it = map
            for (auto jt = it->begin(); jt != it->end(); jt++) {
                // jt = map entry (key: value)
                //if (jt->second == newListHash) continue;
                if (editDist(*kt, jt->first) <= editDistance) {
                    (*it)[*kt] = newListHash;
                    added = true;
                }
            }
        }
        if (!added) {
            map<string, string> init;
            init[*kt] = newListHash;
            results.push_back(init);
        }
    }
    return results;
}



int main(int argc, char** argv)
{
    cout << endl << "-----------Fn Start-----------" << endl;
    cout << "SEARCH" << endl;

    bool search3 = true;
    bool textSearch = false;
    bool useKeywords = false;
    bool searchCommon = false;
    int editDistance = 2;

    vector<string> keywords;
    vector<string> hashes;
    map <string, std::set<string>> hashResults;

    auto is_hex = [](const string& arg) {
        return regex_match(arg, regex("(0x)?[a-fA-F0-9]{8}"));
    };

    group cli = (
        (
            joinable(
                option("-t", "--text").set(textSearch) % "Search by text value instead of hash",
                option("-e", "--search3").set(search3, false) % "Search 3 word table too"
            ),
            values("values", hashes) % "Values to search for",
            (option("-k", "--keywords").set(useKeywords) % "Highlight any occurences of provided keywords in printed results"
                & values("keywords", keywords) % "Keywords to highlight")
            )
        |
        command("common").set(searchCommon) % "Search for strings within a certain edit distance of each other across different hash searches",
        option("-d", "--distance") & number("distance", editDistance) % "Edit distance threshold for common strings",
        option("-h", "--hashes") & values(is_hex, "hashes", hashes) % "Hashes to search for and then compare results"
        );


    parsing_result res = parse(argc, argv, cli);
    if (res.any_error()) {
        cout << endl << make_man_page(cli, "Search") << endl;
        return 1;
    }
    if (!textSearch && !all_of(hashes.begin(), hashes.end(), is_hex)) {
        cout << endl << "\033[31mHash mode must receive valid hexidecimal hash\033[0m" << endl;
        cout << endl << make_man_page(cli, "Search") << endl;
        return 1;
    }
    if (searchCommon && hashes.size() < 2) {
        cout << endl << "\033[31mMust prodive at least two hashes to find common strings of\033[0m" << endl;
        cout << endl << make_man_page(cli, "Search") << endl;
        return 1;
    }

    function<void(pqxx::row::reference)> printer = [&useKeywords, &keywords](pqxx::row::reference text) {
        if (useKeywords) {
            for (auto it = keywords.begin(); it != keywords.end(); it++) {
                if (text.as<string>().find(*it) != string::npos) {
                    cout << "\033[32m" << text << "\033[0m" << endl;
                    return;
                }
            }
        }
        cout << text << endl;
    };

    pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };


    if (textSearch)
    {
        cout << "TEXT mode" << endl;
        cout << "Searching for: " << iterStr(hashes) << endl;
    }
    else if (searchCommon)
    {
        cout << "COMMON mode" << endl;
        cout << "Searching for strings within edit distance " << editDistance << " from the results of these hashes:" << endl;
        for (auto it = hashes.begin(); it != hashes.end(); it++)
        {
            hashResults[*it] = std::set<string>();
            cout << *it << endl;
        }
    }
    else
    {
        cout << "HASH mode" << endl;
        cout << "Searching for: " << endl;
        for (auto it = hashes.begin(); it != hashes.end(); it++) {
            int iHash = ((int)strtol((*it).c_str(), NULL, 16));
            cout << std::hex << "\033[31m" << iHash << "\033[0m" << endl;
        }
    }

    int results = 0;

    for (auto it = hashes.begin(); it != hashes.end(); it++) {
        string input = *it;
        string searchStr;
        cout << endl << "\033[31m" << std::hex << input << ":\033[0m" << endl;
        if (!textSearch) {
            int hash = ((int)strtol((*it).c_str(), NULL, 16));
            searchStr = to_string(hash);
        }
        else {
            searchStr = c.quote(input);
        }
        string retField = textSearch ? "hash" : "text";
        string searchField = textSearch ? "text" : "hash";

        pqxx::work wparts(c);
        pqxx::result rparts = wparts.exec("SELECT " + retField + ", source FROM labels_parts WHERE " + searchField + " = " + searchStr + " ORDER BY quality ASC");
        wparts.commit();

        if (rparts.size() != 0) {
            if (!searchCommon) cout << endl << "parts:" << endl;
            for (auto const& row : rparts) {
                results++;
                if (textSearch) {
                    cout << std::hex << row[0].as<int>() << ": " << row[1] << endl;
                }
                else {
                    if (searchCommon) {
                        hashResults[input].insert(row[0].as<string>());
                    }
                    else {
                        printer(row[0]);
                    }
                }
            }
        }

        pqxx::work wfull(c);
        pqxx::result rfull = wfull.exec("SELECT " + retField + " FROM labels_full WHERE " + searchField + " = " + searchStr + " ORDER BY quality ASC");
        wfull.commit();

        if (rfull.size() != 0) {
            if (!searchCommon) cout << endl << "full:" << endl;
            for (auto const& row : rfull) {
                results++;
                if (textSearch) {
                    cout << std::hex << row[0].as<int>() << endl;
                }
                else {
                    if (searchCommon) {
                        hashResults[input].insert(row[0].as<string>());
                    }
                    else {
                        printer(row[0]);
                    }
                }
            }
        }

        pqxx::work wgen2(c);
        pqxx::result rgen2 = wgen2.exec("SELECT " + retField + " FROM labels_gen2 WHERE " + searchField + " = " + searchStr + " ORDER BY quality ASC, text ASC");
        wgen2.commit();

        if (rgen2.size() != 0) {
            if (!searchCommon) cout << endl << "gen2:" << endl;
            for (auto const& row : rgen2) {
                results++;
                if (textSearch) {
                    cout << std::hex << row[0].as<int>() << endl;
                }
                else {
                    if (searchCommon) {
                        hashResults[input].insert(row[0].as<string>());
                    }
                    else {
                        printer(row[0]);
                    }
                }
            }
        }

        if (search3) {
            pqxx::work wgen3(c);
            pqxx::result rgen3 = wgen3.exec("SELECT " + retField + " FROM labels_gen3 WHERE " + searchField + " = " + searchStr + " ORDER BY quality ASC, text ASC");
            wgen3.commit();

            if (rgen3.size() != 0) {
                if (!searchCommon) cout << endl << "gen3:" << endl;
                for (auto const& row : rgen3) {
                    results++;
                    if (textSearch) {
                        cout << std::hex << row[0].as<int>() << endl;
                    }
                    else {
                        if (searchCommon) {
                            hashResults[input].insert(row[0].as<string>());
                        }
                        else {
                            printer(row[0]);
                        }
                    }
                }
            }
        }
        if (!searchCommon) {
            cout << endl << "Total of " << std::dec << results << " results found for " << input << endl;
        }
    }

    if (searchCommon) {
        vector<map<string, string>> results;
        // [ { string: hash, string: hash }...{}]
        for (uint i = 0; i < hashes.size(); i++) {
            results = searchForCommon(results, hashResults[hashes[i]], hashes[i], editDistance);
        }

        for (auto mt = results.begin(); mt != results.end(); mt++) {
            if (mt->size() >= hashes.size()) {
                cout << endl;
                for (auto el = mt->begin(); el != mt->end(); el++) {
                    cout << el->first << ": " << el->second << endl;
                }
            }
        }
    }

    cout << "-----------Fn End-----------" << endl;
    return 0;
}

