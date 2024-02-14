#ifndef LFV_EXCEPTION

#define LFV_EXCEPTION

#include <exception>
#include <string>

class LFVException: public std::exception {
public:
  LFVException(const std::string& reason): m_reason(reason) {
  }

  const char* what() const _NOEXCEPT { return m_reason.c_str(); }

private:
  std::string m_reason;
};

#endif
