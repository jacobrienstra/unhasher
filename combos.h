#ifndef COMBOS_H
#define COMBOS_H

#include "types.h"
#include "hasher.h"

std::set<std::pair<std::string, unsigned int> > findNewPartCombos(std::set<std::string>& newStrs, int numWords, Corpus& corp, SearchHashes& searchCorp, bool insert = true, int quality = 0);
void genAllCombos(int numWords, Corpus& corp, SearchHashes& searchCorp, bool insert = true);

#endif