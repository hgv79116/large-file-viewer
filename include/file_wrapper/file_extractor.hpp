#include "stream_wrapper/stream_wrapper.hpp"
#include "util/file_metadata.hpp"
#include "util/lfv_exception.hpp"

#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <string>
#include <vector>

struct FileSegment {
  std::streampos begin_pos;
  std::streampos end_pos;
  std::string content;
};

class FileLineExtractor {
public:
  FileLineExtractor(std::unique_ptr<std::ifstream> stream_ptr, size_t stream_size)
    : _stream_w{std::move(stream_ptr)},
      _stream_size{stream_size} {
    }

  std::streampos get_line_begin(std::streampos pos) {
    if (pos == 0) {
      // If pos is at the beginning of the file
      return 0;
    }

    auto prev_line_end = _stream_w.find_last_of('\n', pos - (std::streamoff)1);
    if (prev_line_end == -1) {
      // This is the first line
      return 0;
    }

    return prev_line_end + (std::streamoff)1;
  }

  std::streampos get_line_end(std::streampos pos) {
    auto this_end_line = _stream_w.find_first_of('\n', pos);

    if (this_end_line == -1) {
      // This is the last sentence, without a terminating '\n'
      return getStreamEnd();
    }

    // Includes the end of line character
    return this_end_line + (std::streamoff)1;
  }

  FileSegment get_line_containing(std::streampos pos) {
    auto line_begin = get_line_begin(pos);
    auto line_end = get_line_end(pos);

    return {line_begin, line_end, _stream_w.slice(line_begin, line_end)};
  }

  FileSegment get_line_from(std::streampos line_begin) {
    auto line_end = get_line_end(line_begin);
    return {line_begin, line_end, _stream_w.slice(line_begin, line_end)};
  }

  std::streampos getStreamEnd() const { return static_cast<std::streampos>(_stream_size); }

private:
  StreamWrapper _stream_w;
  size_t _stream_size;
};