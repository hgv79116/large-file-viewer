#include <string>
#include <vector>

class SafeArg {
public:
  SafeArg(const std::string& argv);
  SafeArg(const SafeArg& rhs) = delete;
  SafeArg(SafeArg&& rhs) = delete;
  auto operator=(SafeArg&& rhs) -> SafeArg& = delete;
  auto operator=(const SafeArg& rhs) -> SafeArg& = delete;

  [[nodiscard]] auto get_argc() const -> int;

  [[nodiscard]] auto get_argv() const -> const char* const*;

  ~SafeArg();

private:
  int m_argc;
  char** m_argv;
};