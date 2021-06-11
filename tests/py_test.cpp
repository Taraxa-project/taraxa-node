#include <filesystem>

#include "unistd.h"

int main(int, char **) {
  return execl(                                                                   //
      "/bin/bash", "/bin/bash",                                                   //
      (std::filesystem::path(__FILE__).parent_path() / "py" / "run.sh").c_str(),  //
      (std::string("--node_executable_path=") + NODE_EXECUTABLE_PATH).c_str(),    //
      (char *)nullptr                                                             //
  );
}