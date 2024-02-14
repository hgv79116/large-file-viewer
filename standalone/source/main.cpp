#include <iostream>
#include <app.hpp>
#include <exception>

auto main(int argc, char** argv) -> int {
  if (argc < 2) {
    std::cerr << "Missing file path";
    return 0;
  }

  const std::string fpath(argv[1]);

  try {
    run_app(fpath);
  } catch (std::exception e) {
    std::cout << e.what();
  } catch (...) {
    std::cout << "Unknown error";
  }

  return 0;
}
