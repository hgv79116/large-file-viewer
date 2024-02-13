#include <cxxopts.hpp>
#include <app.hpp>

auto main() -> int {
  const std::string fpath = "/Users/hoanggiapvu/Documents/very large text file.txt";

  run_app(fpath);

  return 0;
}
