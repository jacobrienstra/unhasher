#include <limits>
#include <iostream>
#include <iomanip>

using namespace std;

int main()
{
  int hash = 5487;
  cout << std::hex << std::setw(8) << std::setfill('0') << hash << endl;

}
