#include "top.hpp"

int main(int argc, const char* argv[]) {
  TaraxaStackTrace st;
  Top top(argc, argv);
  if (top.isActive()) {
    top.run();
  }
  std::cout << "Taraxa Node exited ..." << std::endl;
}
