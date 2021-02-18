#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <pqxx/pqxx>

#include "hasher.h"
#include "combos.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/random_access_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>


using namespace ::boost;
using namespace ::boost::multi_index;
using namespace ::boost::asio;
using namespace std;

typedef unsigned int uint;
typedef unsigned long ulong;


// Check for only successful inserts (shouldn't be necessarily if entirely new)

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
> UnknownHashes;

typedef Corpus::random_access_index RandIndex;
typedef Corpus::random_access_index::iterator RandIter;
typedef Corpus::index<Word>::type WordIndex;
typedef Corpus::index<Word>::type::iterator WordIter;
typedef UnknownHashes::index<Hash>::type SearchIndex;
typedef UnknownHashes::index<Hash>::type::iterator SearchIter;

class MainContext {
public:
  int numWords;
  ulong numThreads;
  Corpus* corp;
  UnknownHashes* searchCorp;
  WordIndex* corpWordi;
  ulong cSize;

  ulong globalMax;
  ulong* counters;
  ulong* foundCounters;

  bool useNewCorp;
  Corpus* newCorp;

  mutex* coutLock;

  MainContext(int pnumWords, ulong pnumThreads,
    Corpus* pcorp, UnknownHashes* psearchCorp,
    ulong pglobalMax,
    ulong* pcounters, ulong* pfoundCounters, mutex* pcoutLock)
  {
    numWords = pnumWords;
    numThreads = pnumThreads;
    corp = pcorp;
    searchCorp = psearchCorp;
    corpWordi = &corp->get<Word>();
    cSize = corp->size();

    globalMax = pglobalMax;
    counters = pcounters;
    foundCounters = pfoundCounters;

    useNewCorp = false;
    coutLock = pcoutLock;
  }

  void setNewCorp(Corpus* pnewCorp) {
    useNewCorp = true;
    newCorp = pnewCorp;
  }
};

class ThreadContext {
public:

  ulong tIndex;
  ulong elIndex;
  int specDepth;
  ulong threadMax;

  pqxx::work* wThread;
  pqxx::pipeline* pThread;


  ThreadContext(
    ulong ptIndex,
    ulong pelIndex,
    int pspecDepth,
    ulong pthreadMax,
    pqxx::work* pwThread,
    pqxx::pipeline* ppThread) {
    tIndex = ptIndex;
    elIndex = pelIndex;
    specDepth = pspecDepth;
    threadMax = pthreadMax;
    wThread = pwThread;
    pThread = ppThread;
  }
};
/**
 *
 * Global vars
 *
**/
int Num_Threads = thread::hardware_concurrency();

/**
 *
 * Printing functions
 *
**/

/***********************************/
// Prints iterable object like array
string iterStr(Corpus& iter) {
  string ret = "[";
  for (auto it = iter.begin(); it != iter.end(); it++) {
    if (it != iter.begin()) {
      ret += ", ";
    }
    ret += it->word;
  }
  ret += "]";
  return ret;
}
string iterStr(set<string>& iter) {
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
/* END printIter */
/*****************/

/****************************/
// Prints percentage complete
void printPerc(ulong* counters, ulong maxCombos, ulong size)
{
  ulong counted = 0;
  for (ulong i = 0; i < size; i++) {
    counted += counters[i];
  }
  double perc = 100.0f * (counted) / maxCombos;
  cout << "\r" << "Combos tried: " << std::fixed << std::setprecision(3) << perc << "%" << flush;
};
/* END printPerc */
/*****************/


/**
 *
 * Database functions
 *
**/

/****************************/
/* Clears corpus of parts */
void clearDB(Corpus& corp)
{
  corp.clear();
  cout << "Corpus cleared" << endl;
}
/* END clearDB */
/***************/

/************************************/
// Loads parts and labels into corpus
// and unknown hashes into the search corpus numWords
void loadDB(int numWords, Corpus& corp, UnknownHashes& searchCorp, bool silent = true)
{
  cout << endl << "LOAD DB" << endl;
  if (!silent) cout << "Connecting to db..." << endl;
  pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };

  if (!silent) cout << "Fetching parts..." << endl;
  pqxx::work w(c);
  pqxx::result r;
  string condition = numWords == 3 ? "WHERE NOT source = '{VALUE}'" : "";
  string query = "SELECT text, hash FROM labels_parts " + condition + " ORDER BY count DESC, source ASC";
  r = w.exec(query);
  w.commit();

  if (!silent) cout << "Loading " << r.size() << " parts into corpus..." << endl;
  for (pqxx::row const& row : r) {
    corp.get<Word>().insert(WordPart(row[0].c_str(), (uint)row[1].as<int>()));
  }
  if (!silent) cout << "Corpus is " << corp.size() << " large" << endl;

  if (numWords == 2)
  {
    if (!silent) cout << "Fetching full..." << endl;
    pqxx::work w2(c);
    pqxx::result r2 = w2.exec("SELECT text, hash FROM labels_full ORDER BY source ASC");
    w2.commit();
    if (!silent) cout << "Loading " << r2.size() << " full into corpus..." << endl;
    for (auto const& row : r2) {
      corp.get<Word>().insert(WordPart(row[0].c_str(), (uint)row[1].as<int>()));
    }
    if (!silent) cout << "Corpus is " << corp.size() << " large" << endl;
  }

  if (!silent) cout << "Loading unknown hashes..." << endl;
  pqxx::work w3(c);
  pqxx::result r3 = w3.exec("SELECT hash FROM unknown_hashes");
  w3.commit();
  for (auto const& row : r3) {
    searchCorp.insert(UnkHash((uint)row[0].as<int>()));
  }
  if (!silent) cout << searchCorp.size() << " unknown hashes loaded" << endl;
  cout << "Done loading db" << endl << endl;;
  return;
}
/* END loadDB */
/*************/

/**************************/
// Recursive comboGenerator
void genCombosRec(
  MainContext& context, ThreadContext& threadContext,
  int depth, uint curHash, string curString
)
{
  if (depth == 0) return;
  Corpus* curIndex = context.corp;
  // If this is the special depth, we only use our limited corpus for this level
  Corpus justThisEl;
  if ((depth == threadContext.specDepth) && context.useNewCorp) {
    WordPart el = context.newCorp->at(threadContext.elIndex);
    justThisEl.get<Word>().insert(el);
    curIndex = &(justThisEl);
  }

  // Loop through index
  for (RandIter it = curIndex->begin(); it != curIndex->end(); it++) {
    string w = it->word;
    uint newHash = w.empty() ? it->hash : hasher(w.c_str(), curHash);
    string newWord = curString + w;

    // Can only be a new combo if last level
    if (depth == 1) {

      // Found a match
      if (context.searchCorp->find(newHash) != context.searchCorp->end()) {
        (*(context.foundCounters + threadContext.tIndex))++;
        try {
          // context.coutLock->lock();
          // cout << newWord << endl;
          // context.coutLock->unlock();

          threadContext.pThread->insert("INSERT INTO labels_gen" + to_string(context.numWords) + " (text, hash) VALUES (" + threadContext.wThread->quote(newWord) + ", " + to_string((int)newHash) + ") ON CONFLICT DO NOTHING");
        }
        catch (std::exception const& e) {
          context.coutLock->lock();
          cout << endl << e.what() << endl;
          context.coutLock->unlock();
        }
      }

      // Inc counter
      ulong c = ++(*(context.counters + threadContext.tIndex));
      if ((c & 4095) == 0 || c == threadContext.threadMax) {
        context.coutLock->lock();
        printPerc(context.counters, context.globalMax, context.numThreads);
        context.coutLock->unlock();
      }
    }
    // Next level downward
    genCombosRec(context, threadContext,
      depth - 1, newHash, newWord);

    // Underscore before only the last word
    if (depth == 2) {
      uint newerHash = hasher("_", newHash);
      genCombosRec(context, threadContext,
        depth - 1, newerHash, newWord + "_");
    }
  }
}
/* END comboGen */
/****************/

/**************************************************************/
// Thread helper function that manages per-thread db connection 
// and calls the recursive combo generator
void threadHelper(
  ulong tIndex,
  MainContext& context) {

  // Stats
  ulong threadMax = pow(context.cSize, context.numWords - 1) * 2;
  // set initial position, hence numWords - 1 positions to test

 // Connection
  pqxx::connection cThread{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };
  pqxx::work wThread(cThread);
  pqxx::pipeline pThread(wThread);

  ulong elIndex = tIndex;
  int specDepth = 0;
  if (context.useNewCorp) {
    elIndex = tIndex % context.newCorp->size();
    specDepth = (tIndex / context.newCorp->size()) + 1;
  }
  ThreadContext threadContext(tIndex, elIndex, specDepth, threadMax, &wThread, &pThread);

  // Normal, all combos
  if (!context.useNewCorp) {
    WordPart threadEl = context.corp->at(elIndex);
    genCombosRec(
      context, threadContext,
      context.numWords - 1, threadEl.hash, threadEl.word
    );
    if (context.numWords == 2)
    {
      uint newerHash = hasher("_", threadEl.hash);
      genCombosRec(
        context, threadContext,
        context.numWords - 1, newerHash, threadEl.word + "_"
      );
    }
  }
  else {
    genCombosRec(
      context, threadContext,
      context.numWords, 5381, ""
    );
  }

  try {
    pThread.complete();
    ulong checked = 0;
    while (!pThread.empty()) {
      pThread.retrieve();
      checked++;
    }
    wThread.commit();
    // context.coutLock->lock();
    // cout << endl << "Thread instance #" << tIndex << " done, confirmed " << checked << " inserted results" << endl << flush;
    // context.coutLock->unlock();
  }
  catch (std::exception const& e)
  {
    cout << endl << e.what() << endl;
  }
  return;
};
/* END threadHelper */
/********************/




/**
 *
 *
 *  Main functions
 *
**/
/*******************************************************/
// Finds all combos possible with each of the new strings
set<pair<string, uint> > findNewPartCombos(set<string>& newStrs, int numWords)
{
  cout << endl << "-----------Fn Start-----------" << endl;
  auto start = chrono::high_resolution_clock::now();
  cout << "FIND NEW PART COMBOS: " << std::dec << numWords << " WORDS" << endl;
  cout << "strings: " << iterStr(newStrs) << endl;

  mutex coutLock;
  Corpus corp;
  UnknownHashes searchCorp;

  loadDB(numWords, corp, searchCorp);
  WordIndex& corpWordi = corp.get<Word>();

  // Special corpus with only new (required) parts
  Corpus newCorp;
  for (string const& st : newStrs)
  {
    if (corpWordi.find(st) != corpWordi.end()) {
      cout << "'" << st << "' already in db; skipping" << endl;
      continue;
    }
    uint strHash = hasher(st.c_str(), 5381);
    newCorp.push_back(WordPart(st, strHash));
  }

  ulong nSize = newCorp.size();
  ulong cSize = corp.size();
  ulong globalMax = pow(cSize, (numWords - 1)) * numWords * nSize * 2;

  // Counter array
  // Each level is a part at a depth
  ulong numThreads = nSize * numWords;
  ulong counters[numThreads];
  for (ulong& counter : counters) counter = 0;
  ulong foundCounters[numThreads];
  for (ulong& counter : foundCounters) counter = 0;

  MainContext context(numWords, numThreads, &corp, &searchCorp, globalMax, counters, foundCounters, &coutLock);
  context.setNewCorp(&newCorp);

  cout << globalMax << " possible combos of " << iterStr(newCorp) << " amid " << numWords - 1 << " other part(s)" << endl;
  cout << "Beginning combo generator..." << endl << endl;


  asio::thread_pool pool(Num_Threads);


  /** We want to try the required string(s) in each position
    * Each required string gets a thread per level
    * elIndex = i % nSize;
    * level =  (i / nSize) + 1;
  **/
  for (ulong i = 0; i < numThreads; i++) {
    post(pool, std::bind(threadHelper, i, context));
  }



  pool.join();

  auto stop = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);

  cout << "\nTime: " << std::fixed << std::setprecision(3) << (double)(duration.count() / 1000000.0f) << " seconds" << endl;

  ulong globalFound = 0;
  for (ulong i = 0; i < numThreads; i++) {
    globalFound += foundCounters[i];
  }

  cout << "Found and inserted " << globalFound << " new combos!" << endl;

  // Return only the strings we used
    // Connection
  pqxx::connection c{ "user=jacob host=localhost port=5432 dbname=dai connect_timeout=10" };
  pqxx::work w(c);
  pqxx::pipeline p(w);

  set<pair<string, uint> > usedStrs;
  for (RandIter it = newCorp.begin(); it != newCorp.end(); it++) {
    usedStrs.insert(pair(it->word, it->hash));
  }

  cout << endl << "Combo generation done!" << endl;
  cout << "-----------Fn End-----------" << endl;
  return usedStrs;
}
/* END findNewPartCombos */
/*************************/


/************************************************/
/* Generate all possible combos from the corpus */
void genAllCombos(int numWords)
{
  cout << endl << "-----------Fn Start-----------" << endl;
  auto start = chrono::high_resolution_clock::now();
  cout << "ALL COMBOS" << endl << endl;

  mutex coutLock;
  Corpus corp;
  UnknownHashes searchCorp;

  loadDB(numWords, corp, searchCorp);


  ulong cSize = corp.size();
  ulong globalMax = pow(cSize, numWords) * 2;

  // Counter array
  ulong counters[cSize];
  for (ulong& counter : counters) counter = 0;
  ulong foundCounters[cSize];
  for (ulong& counter : foundCounters) counter = 0;

  MainContext context(numWords, cSize, &corp, &searchCorp, globalMax, counters, foundCounters, &coutLock);

  cout << std::dec << numWords << " WORDS" << endl;
  cout << globalMax << " possible combinations of " << numWords << " parts" << endl;
  cout << "Beginning combo generator..." << endl << endl;

  asio::thread_pool pool(Num_Threads);

  for (ulong i = 0; i < cSize; i++) {
    post(pool, std::bind(threadHelper, i, context));
  }

  pool.join();

  auto stop = chrono::high_resolution_clock::now();
  auto duration = chrono::duration_cast<chrono::microseconds>(stop - start);

  cout << "\nTime: " << std::fixed << std::setprecision(3) << (double)(duration.count() / 1000000.0f) << " seconds" << endl;

  ulong globalFound = 0;
  for (ulong i = 0; i < cSize; i++) {
    globalFound += foundCounters[i];
  }

  cout << globalFound << " matches for unknown hashes found!" << endl;

  cout << endl << "Combo generation done!" << endl;
  cout << "-----------Fn End-----------" << endl;
  return;
}
/* END allCombos */
/*****************/


