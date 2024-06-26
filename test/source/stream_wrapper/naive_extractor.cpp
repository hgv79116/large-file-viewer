#include "stream_wrapper/naive_extractor.hpp"

#include <doctest/doctest.h>

#include <memory>
#include <sstream>
#include <string>
#include <iostream>

void validateExtractor(std::string content, int start, int length, std::string sub) {
  NaiveContentExtractor extr (
    std::make_unique<std::stringstream>(content),
    length);
  extr.setStartPos(start);
  for (auto v: extr.extract()) {
    std::cout << int(v) << std::endl;
  }
  CHECK(extr.extract() == sub);
}

TEST_CASE("Test naive extractor") { validateExtractor("abcdef", 2, 2, "cd"); }
TEST_CASE("Test naive extractor, overflow extraction") { validateExtractor("abcdef", 2, 100, "cdef"); }