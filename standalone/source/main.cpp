#include <LFV/app.hpp>
#include <exception>
#include <iostream>

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Missing file path";
    return 0;
  }

  const std::string fpath(argv[1]);

  try {
    run_app(fpath);
  } catch (std::exception const& e) {
    std::cerr << e.what();
  } catch (...) {
    std::cerr << "Unknown error";
  }

  return 0;
}
