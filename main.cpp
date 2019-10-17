#include "top.hpp"

int main(int argc, const char* argv[]) {
  TaraxaStackTrace st;
  Top top(argc, argv);
  top.join();
  std::cout << "Taraxa Node exited ..." << std::endl;
}
