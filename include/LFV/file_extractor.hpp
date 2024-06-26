#include "stream_wrapper.hpp"
#include "file_metadata.hpp"

#include <LFV/lfv_exception.hpp>
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

class EditWindowExtractor {
public:
  EditWindowExtractor(std::unique_ptr<std::ifstream> stream_ptr, FileMetadata fmeta)
      : _file_line_extr{std::move(stream_ptr), fmeta.getSize()} {
    load_initial_file_content();
  }

  void set_size(int width, int height) {
    _width = width;
    _height = height;
    _anchor = get_window_begin();

    reset();

    load_initial_file_content();
  }

  void move_to(std::streampos pos) {
    _anchor = pos;

    reset();

    load_initial_file_content();
  }

  std::streampos getStreamEnd() const { return _file_line_extr.getStreamEnd(); }

  bool can_move_down() {
    if (_line_offset + _height < static_cast<int>(_splitted_lines.size())) {
      return true;
    }

    return can_extract_next_raw_line();
  }

  bool can_move_up() {
    if (_line_offset > 0) {
      return true;
    }

    return can_extract_prev_raw_line();
  }

  void move_down() {
    if (_line_offset + _height >= static_cast<int>(_splitted_lines.size())) {
      add_next_raw_line();
    }

    _line_offset++;

    cut_redundant_front_lines();
  }

  void move_up() {
    if (_line_offset == 0) {
      add_prev_raw_line();
    }

    _line_offset--;
    cut_redundant_back_lines();
  }

  std::vector<std::string> get_lines() {
    auto end_line_offset = std::min(static_cast<int>(_line_offset + _height),
                                    static_cast<int>(_splitted_lines.size()));

    return std::vector<std::string>{begin(_splitted_lines) + _line_offset,
                                    begin(_splitted_lines) + end_line_offset};
  }

  std::streampos get_streampos() { return get_window_begin(); }

private:
  // Should be no more than 80
  struct RawLine {
    FileSegment line;
    int begin_offset;
    int end_offset;
  };

  FileLineExtractor _file_line_extr;

  // width and height are required to be positive
  int _width = 1;
  int _height = 1;

  // The stream position that the loaded content is anchored around
  std::streampos _anchor = 0;

  // INTERNAL DATA STRUCTURES

  // _splitted_lines and _raw_lines need to be kept sync
  std::vector<std::string> _splitted_lines;

  // After construction, _raw_lines should not be empty except for the case
  // when the file is empty
  std::deque<RawLine> _raw_lines;

  int _line_offset = 0;

  void reset() {
    // Reset all internal data structures
    _splitted_lines.clear();
    _raw_lines.clear();
    _line_offset = 0;
  }

  void load_initial_file_content() {
    while (can_extract_next_raw_line() && static_cast<int>(_splitted_lines.size()) < _height) {
      add_next_raw_line();
    }
  }

  void cut_redundant_front_lines() {
    while (!_raw_lines.empty() && _raw_lines.front().end_offset <= _line_offset) {
      int begin_offset = _raw_lines.front().begin_offset;
      int end_offset = _raw_lines.front().end_offset;

      int to_be_removed_num = end_offset - begin_offset;

      for (auto& raw_line : _raw_lines) {
        raw_line.begin_offset -= to_be_removed_num;
        raw_line.end_offset -= to_be_removed_num;
      }

      _raw_lines.pop_front();

      _splitted_lines.erase(begin(_splitted_lines) + begin_offset,
                             begin(_splitted_lines) + end_offset);

      _line_offset -= to_be_removed_num;
    }
  }

  void cut_redundant_back_lines() {
    while (!_raw_lines.empty() && _raw_lines.back().begin_offset >= _line_offset + _height) {
      int begin_offset = _raw_lines.back().begin_offset;
      int end_offset = _raw_lines.back().end_offset;

      _raw_lines.pop_back();

      _splitted_lines.erase(begin(_splitted_lines) + begin_offset,
                             begin(_splitted_lines) + end_offset);
    }
  }

  void add_next_raw_line() {
    auto next_raw_line = extract_next_raw_line();

    auto next_line_splitted = split_line(next_raw_line.content);

    int begin_offset = _splitted_lines.size();
    int end_offset = _splitted_lines.size() + next_line_splitted.size();

    // Update all internal data structures

    // Insert to the end of splitted lines
    _splitted_lines.insert(end(_splitted_lines), begin(next_line_splitted),
                            end(next_line_splitted));

    // Insert to the end of raw lines
    _raw_lines.push_back({next_raw_line, begin_offset, end_offset});
  }

  void add_prev_raw_line() {
    auto prev_raw_line = extract_prev_raw_line();

    auto prev_line_splitted = split_line(prev_raw_line.content);

    size_t new_line_num = prev_line_splitted.size();
    if (new_line_num > std::numeric_limits<int>::max()) {
      throw LFVException("Line length exceeded limit");
    }

    auto prepended_line_offset = static_cast<int>(new_line_num);

    for (auto& raw_line : _raw_lines) {
      raw_line.begin_offset += prepended_line_offset;
      raw_line.end_offset += prepended_line_offset;
    }

    // Update all internal data structures

    // Insert into the beginning of splitted lines
    _splitted_lines.insert(begin(_splitted_lines), begin(prev_line_splitted),
                            end(prev_line_splitted));

    // Insert into the beginning of raw lines
    _raw_lines.push_front({prev_raw_line, 0, prepended_line_offset});

    // Push the offset back
    _line_offset += new_line_num;
  }

  FileSegment extract_prev_raw_line() {
    return _file_line_extr.get_line_containing(get_window_begin() - (std::streamoff)1);
  }

  FileSegment extract_next_raw_line() {
    return _file_line_extr.get_line_containing(get_window_end());
  }

  bool can_extract_next_raw_line() { return get_window_end() < _file_line_extr.getStreamEnd(); }

  bool can_extract_prev_raw_line() { return get_window_begin() > 0; }

  std::vector<std::string> split_line(const std::string& line, const char sep = ' ') const {
    std::vector<std::string> ret;

    for (size_t i = 0; i < line.size();) {
      // Take the rest of the line if possible
      uint32_t width = line.size() - i;

      if (i + _width < line.size()) {
        // Otherwise, take until the last separator if there is one
        int next_space = line.find_last_of(sep, i + _width - 1);

        if (next_space == -1 || (next_space >= 0 && (unsigned)next_space < i)) {
          // No space before next cut
          // We just simply cut at that point.
          // In extreme cases a word will be seperated
          width = _width;
        } else {
          // Cut until the space (inclusively)
          width = next_space + 1 - i;
        }
      }

      ret.push_back(line.substr(i, width));

      i += width;
    }

    return ret;
  }

  std::streampos get_window_begin() {
    if (_raw_lines.empty()) {
      return _anchor;
    }

    return _raw_lines.front().line.begin_pos;
  }

  std::streampos get_window_end() {
    if (_raw_lines.empty()) {
      return _anchor;
    }

    return _raw_lines.back().line.end_pos;
  }
};