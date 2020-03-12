#include "test_system_command.h"

#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace respire {

using StringVector = std::vector<std::string>;

StringVector Split(const std::string& in) {
  StringVector ret;
  std::string::size_type pos = 0;
  std::string::size_type scan_from = 0;

  // By using ",,", it means that ",," is the separator and so you can freely
  // se "," in the body of the text.
  size_t num_commas = 0;

  do {
    std::string::size_type content_end = in.find(
        pos == 0 ? " " : ",", scan_from);
    if (content_end == std::string::npos) {
      ret.push_back(in.substr(pos, std::string::npos));
      break;
    }

    std::string::size_type separator_end = content_end + 1;
    if (pos != 0) {
      while (in[separator_end] == ',') {
        ++separator_end;
      }
    }

    if (separator_end - content_end < num_commas) {
      // False alarm, continue reading the content.
      scan_from = separator_end;
      continue;
    } else {
      num_commas = separator_end - content_end;
    }

    ret.push_back(in.substr(pos, content_end - pos));

    pos = separator_end;
    scan_from = pos;
  } while (true);

  return ret;
}

int EchoCommand(
    const StringVector& tokens,
    const stdext::optional<stdext::file_system::Path>& stdout_file,
    const stdext::optional<stdext::file_system::Path>& stderr_file) {
  assert(tokens.size() == 2);

  const std::string* out_filepath = &tokens[0];
  if (*out_filepath == "stdout") {
    if (!stdout_file) {
      // Send output to null, return trivial success.
      return 0;
    }

    std::cout << "    = " << stdout_file->str() << std::endl;

    out_filepath = &(stdout_file->str());
  } else if (*out_filepath == "stderr") {
    if (!stderr_file) {
      // Send output to null, return trivial success.
      return 0;
    }
    out_filepath = &(stderr_file->str());
  }

  std::ofstream out(*out_filepath);
  if (!out) {
    std::cerr
        << "EchoCommand: Failed to open output: " << *out_filepath << std::endl;
    return 1;
  }
  out << tokens[1];
  return 0;
}

int CopyFileIfDifferent(const StringVector& tokens) {
  assert(tokens.size() == 3);

  const std::string* source = &tokens[0];
  const std::string* destination = &tokens[1];
  const std::string* timestamp = &tokens[2];

  {
    std::ofstream out(*timestamp);
  }

  std::stringstream source_contents;
  {
    std::ifstream in(*source, std::ios::binary);
    if (!in) {
      std::cerr << "CopyFileIfDifferent: Failed to open input: " << *source
                << std::endl;
      return 1;
    }
    source_contents << in.rdbuf();
  }

  if (stdext::file_system::GetLastModificationTime(destination)) {
    // Compare the contents of the two files.
    std::stringstream destination_contents;
    {
      std::ifstream in(*destination, std::ios::binary);
      destination_contents << in.rdbuf();
    }

    if (source_contents.str() == destination_contents.str()) {
      return 0;
    }
  }

  std::ofstream out(*destination);
  if (!out) {
    std::cerr << "CopyFileIfDifferent: Failed to open output: " << *destination
              << std::endl;
    return 1;
  }
  out << source_contents.str();
  return 0;
}

int EchoIfDoesNotExistCommand(
    const StringVector& tokens,
    const stdext::optional<stdext::file_system::Path>& stdout_file,
    const stdext::optional<stdext::file_system::Path>& stderr_file) {
  assert(tokens.size() == 2);
  if (!stdext::file_system::GetLastModificationTime(tokens[0].c_str())) {
    return EchoCommand(tokens, stdout_file, stderr_file);
  } else {
    return 0;
  }
}

int CatCommand(
    const StringVector& tokens,
    const stdext::optional<stdext::file_system::Path>& stdin_file) {
  assert(tokens.size() >= 2);

  std::ofstream out(tokens[0]);
  if (!out) {
    std::cerr
        << "CatCommand: Failed to open output: " << tokens[0] << std::endl;
    return 1;
  }

  for (auto iter = tokens.begin() + 1; iter < tokens.end(); ++iter) {
    const std::string* file_path = &(*iter);
    if (*file_path == "stdin") {
      if (!stdin_file) {
        // If no stdin file is specified, then null is assumed and we append
        // nothing trivially.
        continue;
      }
      file_path = &(stdin_file->str());
    }

    std::ifstream in(*file_path);
    if (!in) {
      std::cerr << "CatCommand: Failed to open input: " << *file_path
                << std::endl;
      return 1;
    }
    out << in.rdbuf();
  }

  return 0;
}

int CatFileListCommand(const StringVector& tokens) {
  assert(tokens.size() == 2);

  std::ofstream out(tokens[0]);
  if (!out) {
    std::cerr
        << "CatFileListCommand: Failed to open output: " << tokens[0]
        << std::endl;
    return 1;
  }

  std::ifstream in(tokens[1]);
  if (!in) {
    std::cerr << "CatFileListCommand: Failed to open input: " << tokens[1]
              << std::endl;
    return 1;
  }
  while (true) {
    std::string line;
    if (!getline(in, line)) {
      break;
    }

    std::ifstream in_sub(line.c_str());
    if (!in_sub) {
      std::cerr << "CatFileListCommand: Failed to open input: " << line.c_str()
                << std::endl;
      return 1;
    }
    out << in_sub.rdbuf();
  }

  return 0;
}

void WaitForFilesystemTimeResolution() {
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
}

int TestSystemCommand(
    const char* command,
    const stdext::optional<stdext::file_system::Path>& stdout_file,
    const stdext::optional<stdext::file_system::Path>& stderr_file,
    const stdext::optional<stdext::file_system::Path>& stdin_file) {
  StringVector tokens(Split(command));

  int ret = 1;

  if (tokens[0] == "echo") {
    ret = EchoCommand(StringVector(tokens.begin() + 1, tokens.end()),
                      stdout_file, stderr_file);
  } else if (tokens[0] == "copy_file_if_different") {
    ret = CopyFileIfDifferent(StringVector(tokens.begin() + 1, tokens.end()));
  } else if (tokens[0] == "cat") {
    ret = CatCommand(
        StringVector(tokens.begin() + 1, tokens.end()), stdin_file);
  } else if (tokens[0] == "cat_file_list") {
    ret = CatFileListCommand(StringVector(tokens.begin() + 1, tokens.end()));
  } else {
    // Unkown command.
    std::cerr << "TestSystemCommand: Unkown command.";
  }

  WaitForFilesystemTimeResolution();
  return ret;
}

}  // namespace respire
