# compiler to use
CC = clang++

# flags to pass compiler
CPPFLAGS = -ggdb3 -O0 -fvisibility=hidden -fvisibility-inlines-hidden -std=c++17 -I/Applications/Postgres.app/Contents/Versions/13/include -Wall -Werror -Wextra

LIBS += -L/Applications/Postgres.app/Contents/Versions/13/lib -lpqxx -lpq

# name for executable
HASH = hash
UNHASH = unhash
LOAD = load
TEST = test


# space-separated list of header files
HASHHDRS = hasher.h combos.h
UNHASHHDRS = hasher.h
LOADHRDS = hasher.h combos.h

# space-separated list of source files
HASHSRCS = hash.cpp hasher.cpp combos.cpp 
UNHASHSRCS = unhash.cpp hasher.cpp
LOADSRCS = load.cpp hasher.cpp combos.cpp 
TESTSRCS = test.cpp

# automatically generated list of object files
HASHOBJS = $(HASHSRCS:.cpp=.o)
UNHASHOBJS = $(UNHASHSRCS:.cpp=.o)
TESTOBJS = $(TESTSRCS:.cpp=.o)
LOADOBJS = $(LOADSRCS:.cpp=.o)

all: $(HASH) $(UNHASH) $(LOAD) $(TEST) 

# default target
$(HASH): $(HASHOBJS) $(HASHHDRS) Makefile
	$(CC) $(CPPFLAGS) -o $@ $(HASHOBJS) $(LIBS)

$(UNHASH): $(UNHASHOBJS) $(UNHASHHDRS) Makefile
	$(CC) $(CPPFLAGS) -o $@ $(UNHASHOBJS) $(LIBS)

$(LOAD): $(LOADOBJS) $(LOADHDRS) Makefile
	$(CC) $(CPPFLAGS)	-o $@ $(LOADOBJS) $(LIBS)

$(TEST): $(TESTOBJS) Makefile
	$(CC) $(CPPFLAGS) -o $@ $(TESTOBJS) $(LIBS)

# dependencies
$(HASHOBJS): $(HASHHRDS) Makefile
$(UNHASHOBJS): $(UNHASHHDRS) Makefile
$(LOADOBJS): $(LOADHDRS) Makefile


# housekeeping
clean:
	rm -f core $(EXE) *.o

