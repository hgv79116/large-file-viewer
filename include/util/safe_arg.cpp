#include "util/safe_arg.hpp"
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

// NOLINTBEGIN
SafeArg::SafeArg(const std::string& argv) {
  std::vector<std::string> v_args;

  int cur = 0;
  for (;;) {
    int first_space = static_cast<int>(argv.find(' ', cur));
    if (first_space == -1) {
      v_args.push_back(argv.substr(cur));
      break;
    }

    // Exclude the space
    v_args.push_back(argv.substr(cur, first_space - cur));
    // Skip the space
    cur = first_space + 1;
  }

  m_argc = v_args.size();
  m_argv = new char*[m_argc];

  for (int i = 0; i < m_argc; i++) {
    const char* arg_c_str = v_args[i].c_str();
    m_argv[i] = new char[strlen(arg_c_str) + 1];
    strcpy(m_argv[i], arg_c_str);
  }
}

int SafeArg::get_argc() const { return m_argc; }

const char* const* SafeArg::get_argv() const { return m_argv; }

SafeArg::~SafeArg() {
  for (int i = 0; i < m_argc; i++) {
    delete[] m_argv[i];
  }

  delete[] m_argv;
}
// NOLINTEND
