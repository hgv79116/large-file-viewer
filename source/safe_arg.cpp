#include <vector>
#include <string>
#include <safe_arg.hpp>
#include <iostream>

SafeArg::SafeArg(const std::string& argv) {
  std::vector<std::string> v_args;

  int cur = 0;
  for (;;) {
    int first_space = argv.find(' ', cur);
    if (first_space == -1) {
      v_args.push_back(argv.substr(cur));
      break;
    }
    else {
      // Exclude the space
      v_args.push_back(argv.substr(cur, first_space - cur));
      // Skip the space
      cur = first_space + 1;
    }
  }

  m_argc = v_args.size();
  m_argv = new char*[m_argc];

  for (int i = 0; i < m_argc; i++) {
    m_argv[i] = strdup(v_args[i].c_str());
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