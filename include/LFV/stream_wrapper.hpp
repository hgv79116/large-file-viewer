#include <istream>
#include <memory>
#include <string>

class StreamWrapper {
public:
  StreamWrapper(std::unique_ptr<std::istream> stream_ptr)
    : _stream_ptr{std::move(stream_ptr)} {
    _stream_ptr->clear();
    _stream_ptr->seekg(0);
  }

  int getc(std::streampos pos) {
    _stream_ptr->clear();
    _stream_ptr->seekg(pos);
    return _stream_ptr->get();
  }

  std::streampos find_first_of(char target, std::streampos pos) {
    _stream_ptr->clear();
    _stream_ptr->seekg(pos);

    while (_stream_ptr->peek() != EOF && _stream_ptr->peek() != target) {
      _stream_ptr->seekg(_stream_ptr->tellg() + std::streamoff{1});
    }

    if (_stream_ptr->peek() == target) {
      return _stream_ptr->tellg();
    }

    return -1;
  }

  std::streampos find_last_of(char target, std::streampos pos) {
    _stream_ptr->clear();
    _stream_ptr->seekg(pos);

    while (_stream_ptr->tellg() > 0 && _stream_ptr->peek() != target) {
      // Some buffers do not support unget
      _stream_ptr->seekg(_stream_ptr->tellg() + std::streamoff{-1});
    }

    if (_stream_ptr->peek() == target) {
      return _stream_ptr->tellg();
    }

    return -1;
  }

  // Undefined behaviour if end is past EOF
  std::string slice(std::streampos begin, std::streampos end) {
    _stream_ptr->clear();
    _stream_ptr->seekg(begin);

    std::string ret;
    while (_stream_ptr->tellg() < end) {
      ret.push_back(_stream_ptr->get());
    }

    return ret;
  }

private:
  std::unique_ptr<std::istream> _stream_ptr;
};