#pragma once

#include <string>
#include <filesystem>

class FileMetadata {
public:
  FileMetadata(std::string fpath):
    _size{std::filesystem::file_size(std::filesystem::path(fpath))},
    _path{fpath}
  {}

  size_t getSize() const { return _size;  }

  std::string getPath() const { return _path;  }

private:
  size_t _size;
  std::string _path;
};