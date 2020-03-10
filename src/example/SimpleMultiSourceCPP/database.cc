#include "database.h"

void Database::insert(const std::string& key, const std::string& value) {
  map_[key] = value;
}

const std::string* Database::lookup(const std::string& key) {
  return &map_[key];
}
