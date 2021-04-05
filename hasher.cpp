#include <string>
#include "hasher.h"

#define M 33

unsigned int hasher(const char* str, unsigned int hashStart)
{
  unsigned int hash = hashStart;
  for (unsigned int i = 0; i < strlen(str); i++)
  {
    hash = ((hash << 5) + hash) ^ (char)str[i];
  }
  return hash;
}