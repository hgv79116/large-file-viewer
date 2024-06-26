#include "content_extractor.hpp"

#include <istream>
#include <memory>

class NaiveContentExtractor: public ContentExtractor {
public:
  NaiveContentExtractor(std::unique_ptr<std::istream> stream_ptr, size_t content_length)
    : _stream_ptr{std::move(stream_ptr)},
      _start_pos{0},
      _len {content_length}
    {}

  void setStartPos(std::streampos pos) override { _start_pos = pos; }

  std::streampos getStartPos() override { return _start_pos; }

  void setContentLength(size_t len) override { _len = len;  }

  size_t getContentLength() override { return _len; }

  std::string extract() override {
    // Clear fail bits to avoid weird stream behaviours
    _stream_ptr->clear();
    _stream_ptr->seekg(_start_pos);
    std::string ret;
    for (size_t index = 0; index < _len && _stream_ptr->peek() != EOF; index++) {
      ret.push_back(_stream_ptr->get());
    }
    return ret;
  }

private:
  std::unique_ptr<std::istream> _stream_ptr;
  std::streampos _start_pos;
  size_t _len;
};