#include <string>
#include <vector>

class SafeArg {
public:
  SafeArg(const std::string& argv);
  SafeArg(const SafeArg& rhs) = delete;
  SafeArg(SafeArg&& rhs) = delete;
  SafeArg& operator=(SafeArg&& rhs) = delete;
  SafeArg& operator=(const SafeArg& rhs) = delete;

  int get_argc() const;

  const char* const* get_argv() const;

  ~SafeArg();

private:
  int m_argc;
  char** m_argv;
};