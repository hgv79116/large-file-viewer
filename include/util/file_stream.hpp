#pragma once

#include <fstream>
#include <memory>

// Throws an exception if fail to open file.
std::unique_ptr<std::ifstream> initialiseFstream(std::string path) {
  auto in_ptr = std::make_unique<std::ifstream>();
  in_ptr->exceptions(std::ifstream::failbit | std::ifstream::badbit);
  in_ptr->open(path);
  return in_ptr;
}