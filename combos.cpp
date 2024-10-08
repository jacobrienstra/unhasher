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

#include "combos.h"

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

using namespace ::boost;
using namespace ::boost::multi_index;
using namespace ::boost::asio;
using namespace std;


// Check for only successful inserts (shouldn't be necessarily if entirely new)

class MainContext {
public:
  int numWords;
  ulong numThreads;
  Corpus* corp;
  SearchHashes* searchCorp;
  WordIndex* corpWordi;
  ulong cSize;

  ulong globalMax;
  ulong* counters;
  ulong* foundCounters;
  ulong* confirmedCounters;

  bool useNewCorp;
  Corpus* newCorp;

  mutex* coutLock;

  bool insert;

  MainContext(int pnumWords, ulong pnumThreads,
    Corpus* pcorp, SearchHashes* psearchCorp,
    ulong pglobalMax,
    ulong* pcounters, ulong* pfoundCounters, ulong* pconfirmedCounters, mutex* pcoutLock, bool pinsert)
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
    confirmedCounters = pconfirmedCounters;

    useNewCorp = false;
    coutLock = pcoutLock;

    insert = pinsert;
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

/**************************/
// Recursive comboGenerator
void genCombosRec(
  MainContext& context, ThreadContext& threadContext,
  int depth, uint curHash, string curString, int curQual, bool used_
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
    int newQual = curQual + it->qual;

    // Can only be a new combo if last level
    if (depth == 1) {

      // Found a match
      if (context.searchCorp->find(newHash) != context.searchCorp->end()) {
        (*(context.foundCounters + threadContext.tIndex))++;
        try {
          // context.coutLock->lock();
          // cout << newWord << endl;
          // context.coutLock->unlock();
          if (context.insert) {
            threadContext.pThread->insert("INSERT INTO labels_gen" + to_string(context.numWords) + " (text, hash, quality) VALUES (" + threadContext.wThread->quote(newWord) + ", " + to_string((int)newHash) + ", " + to_string(newQual) + ") ON CONFLICT (text) DO NOTHING RETURNING text");
          }
          else {
            context.coutLock->lock();
            cout << endl << newWord << " : " << newHash << endl;
            context.coutLock->unlock();
          }
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
      depth - 1, newHash, newWord, newQual, used_);

    // Underscore if not used before
    if (!used_ && depth != 1) {
      uint _Hash = hasher("_", newHash);
      genCombosRec(context, threadContext,
        depth - 1, _Hash, newWord + "_", newQual, true);
      uint spHash = hasher(" ", newHash);
      genCombosRec(context, threadContext,
        depth - 1, spHash, newWord + " ", newQual, true);
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
  ulong threadMax = pow(context.cSize, context.numWords - 1) * context.numWords;
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
    // bool used_ = false;
    // if (threadEl.word.find('_') != string::npos) {
    //   used_ = true;
    // }
    genCombosRec(
      context, threadContext,
      context.numWords - 1, threadEl.hash, threadEl.word, threadEl.qual, false
    );
    uint _Hash = hasher("_", threadEl.hash);
    genCombosRec(
      context, threadContext,
      context.numWords - 1, _Hash, threadEl.word + "_", threadEl.qual, true);
    uint spHash = hasher(" ", threadEl.hash);
    genCombosRec(
      context, threadContext,
      context.numWords - 1, spHash, threadEl.word + " ", threadEl.qual, true);

  }
  else {
    genCombosRec(
      context, threadContext,
      context.numWords, 5381, "", 0, false
    );
  }

  try {
    pThread.complete();
    while (!pThread.empty()) {
      pair<int, pqxx::result> res = pThread.retrieve();
      if (!res.second.empty()) {
        (*(context.confirmedCounters + tIndex))++;
      }
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
set<pair<string, uint> > findNewPartCombos(set<string>& newStrs, int numWords, Corpus& corp, SearchHashes& searchCorp, bool insert, int quality)
{
  cout << endl << "-----------Fn Start-----------" << endl;
  auto start = chrono::high_resolution_clock::now();
  cout << "FIND NEW PART COMBOS: " << std::dec << numWords << " WORDS" << endl;
  cout << "strings: " << iterStr(newStrs) << endl;

  mutex coutLock;
  WordIndex& corpWordi = corp.get<Word>();

  // Special corpus with only new (required) parts
  Corpus newCorp;
  for (string const& st : newStrs)
  {
    if (insert && corpWordi.find(st) != corpWordi.end()) {
      cout << "'" << st << "' already in db; skipping" << endl;
      continue;
    }
    uint strHash = hasher(st.c_str(), 5381);
    newCorp.push_back(WordPart(st, strHash, quality));
    corp.push_back(WordPart(st, strHash, quality));
  }

  ulong nSize = newCorp.size();
  ulong cSize = corp.size();
  ulong globalMax = pow(cSize, (numWords - 1)) * numWords * nSize * ((2 * numWords) - 1);

  // Counter array
  // Each level is a part at a depth
  ulong numThreads = nSize * numWords;
  ulong counters[numThreads];
  for (ulong& counter : counters) counter = 0;
  ulong foundCounters[numThreads];
  for (ulong& counter : foundCounters) counter = 0;
  ulong confirmedCounters[numThreads];
  for (ulong& counter : confirmedCounters) counter = 0;

  MainContext context(numWords, numThreads, &corp, &searchCorp, globalMax, counters, foundCounters, confirmedCounters, &coutLock, insert);
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

  cout << "Found and " << (context.insert ? "inserted " : "printed ") << globalFound << " new combos!" << endl;

  ulong globalConfirmed = 0;
  for (ulong i = 0; i < numThreads; i++) {
    globalConfirmed += confirmedCounters[i];
  }

  cout << "Confirmed " << globalConfirmed << " inserts" << endl;

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
void genAllCombos(int numWords, Corpus& corp, SearchHashes& searchCorp, bool insert)
{
  cout << endl << "-----------Fn Start-----------" << endl;
  auto start = chrono::high_resolution_clock::now();
  cout << "ALL COMBOS" << endl << endl;

  mutex coutLock;

  ulong cSize = corp.size();
  ulong globalMax = pow(cSize, numWords) * ((2 * numWords) - 1); // underscore positions + space positions + neither = numWords

  // Counter array
  ulong counters[cSize];
  for (ulong& counter : counters) counter = 0;
  ulong foundCounters[cSize];
  for (ulong& counter : foundCounters) counter = 0;
  ulong confirmedCounters[cSize];
  for (ulong& counter : confirmedCounters) counter = 0;

  MainContext context(numWords, cSize, &corp, &searchCorp, globalMax, counters, foundCounters, confirmedCounters, &coutLock, insert);

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

  ulong globalConfirmed = 0;
  for (ulong i = 0; i < cSize; i++) {
    globalConfirmed += confirmedCounters[i];
  }

  cout << "Confirmed " << globalConfirmed << " inserts" << endl;

  cout << endl << "Combo generation done!" << endl;
  cout << "-----------Fn End-----------" << endl;
  return;
}
/* END allCombos */
/*****************/


