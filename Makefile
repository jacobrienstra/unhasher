# compiler to use
CC = clang++

# flags to pass compiler
CPPFLAGS = -ggdb3 -O0 -fvisibility=hidden -fvisibility-inlines-hidden -std=c++17 -I/Applications/Postgres.app/Contents/Versions/13/include -Wall -Werror -Wextra

LIBS += -L/Applications/Postgres.app/Contents/Versions/13/lib -lpqxx -lpq

# name for executable
LOAD = load
GEN = gen
HASH = hash
SEARCH = search
TEST = test


# space-separated list of header files
GENHRDS =  types.h loaddb.h combos.h
HASHHDRS = types.h hasher.h
SEARCHHDRS = types.h loaddb.h
TESTHDRS = types.h

# space-separated list of source files
LOADSRCS = load.cpp
GENSRCS = gen.cpp loaddb.cpp combos.cpp hasher.cpp
HASHSRCS = hash.cpp hasher.cpp
SEARCHSRCS = search.cpp loaddb.cpp
TESTSRCS = test.cpp

# automatically generated list of object files
LOADOBJS = $(LOADSRCS:.cpp=.o)
GENOBJS = $(GENSRCS:.cpp=.o)
HASHOBJS = $(HASHSRCS:.cpp=.o)
SEARCHOBJS = $(SEARCHSRCS:.cpp=.o)
TESTOBJS = $(TESTSRCS:.cpp=.o)


all: $(LOAD) $(GEN) $(HASH) $(SEARCH) $(TEST) 

# default target
$(LOAD): $(LOADOBJS) Makefile
	$(CC) $(CPPFLAGS)	-o $@ $(LOADOBJS) $(LIBS)

$(GEN): $(GENOBJS) $(GENHRDS) Makefile
	$(CC) $(CPPFLAGS)	-o $@ $(GENOBJS) $(LIBS)

$(HASH): $(HASHOBJS) $(HASHHDRS) Makefile
	$(CC) $(CPPFLAGS) -o $@ $(HASHOBJS) $(LIBS)

$(SEARCH): $(SEARCHOBJS) $(SEARCHHDRS) Makefile
	$(CC) $(CPPFLAGS) -o $@ $(SEARCHOBJS) $(LIBS)

$(TEST): $(TESTOBJS) $(TESTHDRS) Makefile
	$(CC) $(CPPFLAGS) -o $@ $(TESTOBJS) $(LIBS)

# dependencies
$(GENOBJS): $(GENHRDS) Makefile
$(HASHOBJS): $(HASHHRDS) Makefile
$(SEARCHOBJS): $(SEARCHHDRS) Makefile
$(TESTOBJS): $(TESTHDRS) Makefile


# housekeeping
clean:
	rm -f core $(EXE) *.o

