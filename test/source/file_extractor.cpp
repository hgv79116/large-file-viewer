#include <doctest/doctest.h>
#include <file_extractor.hpp>
#include <string>

TEST_CASE("Test file extractor") {
  const std::string fpath = "/Users/hoanggiapvu/Documents/very large text file.txt";
  auto file_extractor = FileExtractor(fpath);

  CHECK(file_extractor.getc(0) == 'T');
  CHECK(file_extractor.getc(file_extractor.find_first_of('\n', 0)) == '\n');
  CHECK(file_extractor.getc(file_extractor.find_last_of('h', 1000)) == 'h');
  CHECK(file_extractor.slice(5, 9) == "is t");
}