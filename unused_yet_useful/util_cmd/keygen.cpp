#include <libdevcrypto/Common.h>
#include <iostream>

using namespace std;
using namespace dev;

int main(int argc, const char* argv[]) {
  auto key_pair = KeyPair::create();
  cout << key_pair.secret().makeInsecure() << endl;
  cout << key_pair.pub() << endl;
  cout << key_pair.address() << endl;
}