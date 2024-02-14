#include <doctest/doctest.h>
#include <string>
#include <cstring>
#include <iostream>

#include <safe_arg.hpp>

TEST_CASE("Test safe_arg") {
  const std::string arg_str = "jump 0";

  SafeArg safe_arg(arg_str);

  CHECK(safe_arg.get_argc() == 2);
  CHECK(strcmp("jump", safe_arg.get_argv()[0]) == 0);
  CHECK(strcmp("0", safe_arg.get_argv()[1]) == 0);
}