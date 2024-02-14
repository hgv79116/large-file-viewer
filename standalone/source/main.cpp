#include <iostream>
#include <app.hpp>

auto main(int argc, char** argv) -> int {
  if (argc < 2) {
    std::cerr << "Missing file path";
    return 0;
  }

  const std::string fpath(argv[1]);

  run_app(fpath);

  return 0;
}
