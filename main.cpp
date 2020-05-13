#include "static_init.hpp"
#include "top.hpp"

int main(int argc, const char* argv[]) {
  try {
    taraxa::static_init();
    Top top(argc, argv);
    top.join();
  } catch (taraxa::ConfigException e) {
    std::cerr << "Configuration exception: " << e.what() << std::endl;
  }
  std::cout << "Taraxa Node exited ..." << std::endl;
}
