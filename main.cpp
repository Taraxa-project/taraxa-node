#include "top.hpp"

int main(int argc, const char* argv[]) {
  TaraxaStackTrace st;
  Top top(argc, argv);
  top.join();
  std::cout << "Taraxa N ode exited ..." << std::endl;
}
