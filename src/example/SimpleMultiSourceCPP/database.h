#include <map>
#include <string>

class Database {
 public:
  void insert(const std::string& key, const std::string& value);
  const std::string* lookup(const std::string& key);

 private:
  std::map<std::string, std::string> map_;
};
