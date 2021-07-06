#include <filesystem>

#include "unistd.h"

int main(int, char **) {
  return execl(  //
      "/bin/bash", "/bin/bash",
      // test runner script path:
      (std::filesystem::path(__FILE__).parent_path() / "py" / "run.sh").c_str(),
      // test runner args:

      // pytest args (see https://docs.pytest.org/en/6.2.x/usage.html):
      "-s",          //
      "--tb=short",  //

      // testing application specific args:
      (std::string("--node_executable_path=") + NODE_EXECUTABLE_PATH).c_str(),  //

      // end test runner args
      (char *)nullptr);
}