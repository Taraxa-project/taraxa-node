/*
 * @Copyright: Taraxa.io
 * @Author: Chia-Chun Lin
 * @Date: 2018-11-29 15:03:45
 * @Last Modified by: Chia-Chun Lin
 * @Last Modified time: 2019-04-19 14:39:51
 */

#include "top.hpp"

int main(int argc, char *argv[]) {
  Top top(argc, argv);
  if (top.isActive()) {
    top.run();
  }
}
