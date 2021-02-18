#ifndef COMBOS_H
#define COMBOS_H

/**
 * Loads dictionary into memory. Returns true if successful else false.
 */
#define NO_MODE 0
#define MODE_START_STRING 1
#define MODE_REQ_STRING 2
#include <set>

std::set<std::pair<std::string, unsigned int> > findNewPartCombos(std::set<std::string>& newStrs, int numWords);
void genAllCombos(int numWords);

#endif