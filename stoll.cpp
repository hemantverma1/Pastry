// stoll example
#include <iostream>   // std::cout
#include <string>     // std::string, std::stoll

int main ()
{
  std::string str = " 097baccf 462f3a9a 2bdf4998 bfce84e0 c60a318e c31bf120 43c6a37b";

  std::string::size_type sz = 0;   // alias of size_t

  while (!str.empty()) {
    long long ll = std::stoll (str,&sz,16);
    std::cout << str.substr(0,sz) << " interpreted as " << ll << '\n';
    str = str.substr(sz);
  }

  return 0;
}
