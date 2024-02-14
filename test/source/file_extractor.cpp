#include <doctest/doctest.h>

#include <LFV/file_extractor.hpp>
#include <filesystem>
#include <string>

#ifdef LOCAL  // Needed to avoid running during github workflows
TEST_CASE("Test file extractor") {
  const std::string fpath = std::filesystem::current_path() / "sample_text.txt";
  {
    std::ofstream out(fpath);
    for (int line = 0; line < 1000; line++) {
      for (int i = 0; i < 5; i++) {
        out << "This is the " << line << "th line";
      }
      out << '\n';
    }
  }
  // Write to sample file

  // Test from sample file
  auto file_extractor = FileExtractor(fpath);
  CHECK(file_extractor.getc(0) == 'T');
  CHECK(file_extractor.getc(file_extractor.find_first_of('\n', 0)) == '\n');
  CHECK(file_extractor.getc(file_extractor.find_last_of('h', 1000)) == 'h');
  CHECK(file_extractor.slice(5, 9) == "is t");
}
#endif