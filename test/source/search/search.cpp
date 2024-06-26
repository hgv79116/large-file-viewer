#include "search_engine/search.hpp"
#include <doctest/doctest.h>
#include <sstream>
#include <istream>
#include <memory>
#include <iostream>

void validateSearch(std::string content, std::string pattern, std::vector<int> matches /* small strings, no need for streampos*/) {
  Search search (
      std::make_unique<std::stringstream>(content),
      {.pattern{pattern}},
      std::make_unique<TaskLogger>());
  search.start();
  CHECK(search.finished());
  auto result = search.getMatches();

  std::cout << result.size() << std::endl;
  CHECK(result.size() == matches.size());
  for (int index = 0; index < static_cast<int>(matches.size()); index++) {
    CHECK(static_cast<int>(result[index]) == matches[index]);
  }
}

TEST_CASE("Test isolated search") { validateSearch("ababababababababa", "bab", {5, 7, 9, 11, 13}); }
TEST_CASE("Test isolated search, neatly fits in") { validateSearch("abab", "ab", {0, 2}); }
TEST_CASE("Test isolated search's ascii range") { validateSearch("\1\1\1", "\1", {0, 1, 2}); }
TEST_CASE("Test isolated search's ascii range") { validateSearch(std::string(3, 127), std::string(1, 127), {0, 1, 2}); }


