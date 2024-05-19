#ifndef LFV_EXCEPTION

#define LFV_EXCEPTION

#include <exception>
#include <string>

class LFVException : public std::exception {
public:
  LFVException(std::string reason) : m_reason(std::move(reason)) {}

  [[nodiscard]] const char* what() const noexcept override { return m_reason.c_str(); }

private:
  std::string m_reason;
};

#endif
