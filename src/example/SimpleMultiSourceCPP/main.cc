#include <iostream>

#include "database.h"

int main(int argc, const char** args) {
  Database database;

  database.insert("output", "hi");

  std::cout << database.lookup("output")->c_str() << std::endl;

  return 0;
}
