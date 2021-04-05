#ifndef HASH_H
#define HASH_H

#define HASH 5381

unsigned int hasher(const char* str, unsigned int hashStart = HASH);

#endif