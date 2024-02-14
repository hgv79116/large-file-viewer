#include <vector>
#include <string>

class SafeArg {
public:
  SafeArg(const std::string& argv);

  int get_argc() const;

  const char* const* get_argv() const;

  ~SafeArg();
private:
  int m_argc;
  char** m_argv;
};