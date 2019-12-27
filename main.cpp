#include "static_init.hpp"
#include "top.hpp"

int main(int argc, const char* argv[]) {
  taraxa::static_init();
  Top top(argc, argv);
  top.join();
  std::cout << "Taraxa Node exited ..." << std::endl;
}
