#ifndef COMBOS_H
#define COMBOS_H

/**
 * Loads dictionary into memory. Returns true if successful else false.
 */
#define NO_MODE 0
#define MODE_START_STRING 1
#define MODE_REQ_STRING 2
#include <set>
#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/member.hpp>

typedef unsigned int uint;
typedef unsigned long ulong;

using namespace std;
using namespace ::boost;
using namespace ::boost::multi_index;

/**
*
* Struct defs
*
**/
typedef struct WordPart {
  string word;
  uint hash;

  WordPart() {
    word = "";
    hash = 0;
  }
  WordPart(string iWord, uint iHash) {
    hash = iHash;
    word = iWord;
  }

  ~WordPart() {}
} WordPart;

struct Word {};
struct Hash {};

typedef multi_index_container<
  WordPart,
  indexed_by<
  random_access<>,
  hashed_unique<tag<Word>, member <WordPart, string, &WordPart::word> >
  >
> Corpus;

typedef struct UnkHash {
  uint hash;
  UnkHash(uint iHash) {
    hash = iHash;
  }

  ~UnkHash() {}
} UnkHash;

typedef multi_index_container<
  UnkHash,
  indexed_by<
  hashed_unique<tag<Hash>, member <UnkHash, uint, &UnkHash::hash> >
  >
> SearchHashes;

typedef Corpus::random_access_index RandIndex;
typedef Corpus::random_access_index::iterator RandIter;
typedef Corpus::index<Word>::type WordIndex;
typedef Corpus::index<Word>::type::iterator WordIter;
typedef SearchHashes::index<Hash>::type SearchIndex;
typedef SearchHashes::index<Hash>::type::iterator SearchIter;

std::set<std::pair<std::string, unsigned int> > findNewPartCombos(std::set<std::string>& newStrs, int numWords, Corpus& corp, SearchHashes& searchCorp);
void genAllCombos(int numWords, Corpus& corp, SearchHashes& searchCorp);

#endif