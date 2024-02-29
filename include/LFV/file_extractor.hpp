#include <LFV/lfv_exception.hpp>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

class FileExtractor {
public:
  FileExtractor(std::string fpath) {
    // Open with exception thrown if fail
    m_in.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    m_in.open(fpath);

    m_in.seekg(0, std::ios_base::end);
    m_end = m_in.tellg();
    m_in.seekg(0, std::ios_base::beg);
    m_in.clear();
  }

  auto get_end() const -> std::streampos { return m_end; }

  auto getc(std::streampos pos) -> int {
    m_in.clear();
    m_in.seekg(pos);
    return m_in.get();
  }

  auto find_first_of(char target, std::streampos pos) -> std::streampos {
    m_in.clear();
    m_in.seekg(pos);

    while (m_in.tellg() < m_end && m_in.peek() != target) {
      m_in.seekg(1, std::ios_base::cur);
    }

    if (m_in.peek() == target) {
      return m_in.tellg();
    }

    return -1;
  }

  auto find_last_of(char target, std::streampos pos) -> std::streampos {
    std::cerr << " starting find at " << pos << std::endl;

    m_in.clear();
    m_in.seekg(pos);

    while (m_in.tellg() > 0 && m_in.peek() != target) {
      // Some buffers do not support unget
      m_in.seekg(-1, std::ios_base::cur);
    }

    if (m_in.peek() == target) {
      std::cerr << " find ended " << std::endl;

      return m_in.tellg();
    }

    std::cerr << " find ended " << std::endl;

    return -1;
  }

  auto slice(std::streampos begin, std::streampos end) -> std::string {
    m_in.clear();
    m_in.seekg(begin);

    std::string ret;
    while (m_in.tellg() < end) {
      ret.push_back(m_in.peek());
      m_in.seekg(1, std::ios_base::cur);
    }

    return ret;
  }

private:
  std::streampos m_end;
  std::ifstream m_in;
};

struct FileSegment {
  std::streampos begin_pos;
  std::streampos end_pos;
  std::string content;
};

class FileLineExtractor {
public:
  static const char EOF_CHAR = 26;

  FileLineExtractor(std::string fpath) : m_file_extractor(fpath) {}

  auto get_end() const -> std::streampos { return m_file_extractor.get_end(); }

  auto get_line_begin(std::streampos pos) -> std::streampos {
    if (pos == 0) {
      // If pos is at the beginning of the file
      return 0;
    }

    auto prev_line_end = m_file_extractor.find_last_of('\n', pos - (std::streamoff)1);
    if (prev_line_end == -1) {
      // This is the first line
      return 0;
    }

    return prev_line_end + (std::streamoff)1;
  }

  auto get_line_end(std::streampos pos) -> std::streampos {
    auto this_end_line = m_file_extractor.find_first_of('\n', pos);

    if (this_end_line == -1) {
      // This is the last sentence, without a terminating '\n'
      return get_end();
    }

    // Includes the end of line character
    return this_end_line + (std::streamoff)1;
  }

  auto get_line_containing(std::streampos pos) -> FileSegment {
    auto line_begin = get_line_begin(pos);
    auto line_end = get_line_end(pos);

    return {line_begin, line_end, m_file_extractor.slice(line_begin, line_end)};
  }

  auto get_line_from(std::streampos line_begin) -> FileSegment {
    auto line_end = get_line_end(line_begin);
    return {line_begin, line_end, m_file_extractor.slice(line_begin, line_end)};
  }

private:
  FileExtractor m_file_extractor;
};

class EditWindowExtractor {
public:
  EditWindowExtractor(std::string fpath)
      : m_fpath(fpath),
        m_file_line_extractor(fpath),
        m_size(std::filesystem::file_size(std::filesystem::path(fpath))) {
    load_initial_file_content();
  }

  void set_size(int width, int height) {
    m_width = width;
    m_height = height;
    m_anchor = get_window_begin();

    reset();

    load_initial_file_content();
  }

  auto get_fpath() -> std::string { return m_fpath; }

  void move_to(std::streampos pos) {
    m_anchor = pos;

    reset();

    load_initial_file_content();
  }

  auto get_size() const -> std::uintmax_t { return m_size; }

  auto get_end() const -> std::streampos { return m_file_line_extractor.get_end(); }

  auto can_move_down() -> bool {
    if (m_line_offset + m_height < static_cast<int>(m_splitted_lines.size())) {
      return true;
    }

    return can_extract_next_raw_line();
  }

  auto can_move_up() -> bool {
    if (m_line_offset > 0) {
      return true;
    }

    return can_extract_prev_raw_line();
  }

  void move_down() {
    if (m_line_offset + m_height >= static_cast<int>(m_splitted_lines.size())) {
      add_next_raw_line();
    }

    m_line_offset++;

    cut_redundant_front_lines();
  }

  void move_up() {
    if (m_line_offset == 0) {
      add_prev_raw_line();
    }

    m_line_offset--;
    cut_redundant_back_lines();
  }

  auto get_lines() -> std::vector<std::string> {
    auto end_line_offset = std::min(static_cast<int>(m_line_offset + m_height),
                                    static_cast<int>(m_splitted_lines.size()));

    return std::vector<std::string>{begin(m_splitted_lines) + m_line_offset,
                                    begin(m_splitted_lines) + end_line_offset};
  }

  auto get_streampos() -> std::streampos { return get_window_begin(); }

private:
  // Should be no more than 80
  struct RawLine {
    FileSegment line;
    int begin_offset;
    int end_offset;
  };

  std::string m_fpath;

  FileLineExtractor m_file_line_extractor;
  std::uintmax_t m_size;
  // width and height are required to be positive
  int m_width = 1;
  int m_height = 1;

  // The stream position that the loaded content is anchored around
  std::streampos m_anchor = 0;

  // INTERNAL DATA STRUCTURES

  // m_splitted_lines and m_raw_lines need to be kept sync
  std::vector<std::string> m_splitted_lines;

  // After construction, m_raw_lines should not be empty
  // except for the case when the file is empty
  std::deque<RawLine> m_raw_lines;

  int m_line_offset = 0;

  void reset() {
    // Reset all internal data structures
    m_splitted_lines.clear();
    m_raw_lines.clear();
    m_line_offset = 0;
  }

  void load_initial_file_content() {
    while (can_extract_next_raw_line() && static_cast<int>(m_splitted_lines.size()) < m_height) {
      add_next_raw_line();
    }
  }

  void cut_redundant_front_lines() {
    while (!m_raw_lines.empty() && m_raw_lines.front().end_offset <= m_line_offset) {
      int begin_offset = m_raw_lines.front().begin_offset;
      int end_offset = m_raw_lines.front().end_offset;

      int to_be_removed_num = end_offset - begin_offset;

      for (auto& raw_line : m_raw_lines) {
        raw_line.begin_offset -= to_be_removed_num;
        raw_line.end_offset -= to_be_removed_num;
      }

      m_raw_lines.pop_front();

      m_splitted_lines.erase(begin(m_splitted_lines) + begin_offset,
                             begin(m_splitted_lines) + end_offset);

      m_line_offset -= to_be_removed_num;
    }
  }

  void cut_redundant_back_lines() {
    while (!m_raw_lines.empty() && m_raw_lines.back().begin_offset >= m_line_offset + m_height) {
      int begin_offset = m_raw_lines.back().begin_offset;
      int end_offset = m_raw_lines.back().end_offset;

      m_raw_lines.pop_back();

      m_splitted_lines.erase(begin(m_splitted_lines) + begin_offset,
                             begin(m_splitted_lines) + end_offset);
    }
  }

  void add_next_raw_line() {
    auto next_raw_line = extract_next_raw_line();

    auto next_line_splitted = split_line(next_raw_line.content);

    int begin_offset = m_splitted_lines.size();
    int end_offset = m_splitted_lines.size() + next_line_splitted.size();

    // Update all internal data structures

    // Insert to the end of splitted lines
    m_splitted_lines.insert(end(m_splitted_lines), begin(next_line_splitted),
                            end(next_line_splitted));

    // Insert to the end of raw lines
    m_raw_lines.push_back({next_raw_line, begin_offset, end_offset});
  }

  void add_prev_raw_line() {
    auto prev_raw_line = extract_prev_raw_line();

    auto prev_line_splitted = split_line(prev_raw_line.content);

    size_t new_line_num = prev_line_splitted.size();
    if (new_line_num > std::numeric_limits<int>::max()) {
      throw LFVException("Line length exceeded limit");
    }

    auto prepended_line_offset = static_cast<int>(new_line_num);

    for (auto& raw_line : m_raw_lines) {
      raw_line.begin_offset += prepended_line_offset;
      raw_line.end_offset += prepended_line_offset;
    }

    // Update all internal data structures

    // Insert into the beginning of splitted lines
    m_splitted_lines.insert(begin(m_splitted_lines), begin(prev_line_splitted),
                            end(prev_line_splitted));

    // Insert into the beginning of raw lines
    m_raw_lines.push_front({prev_raw_line, 0, prepended_line_offset});

    // Push the offset back
    m_line_offset += new_line_num;
  }

  auto extract_prev_raw_line() -> FileSegment {
    return m_file_line_extractor.get_line_containing(get_window_begin() - (std::streamoff)1);
  }

  auto extract_next_raw_line() -> FileSegment {
    return m_file_line_extractor.get_line_containing(get_window_end());
  }

  auto can_extract_next_raw_line() -> bool {
    return get_window_end() < m_file_line_extractor.get_end();
  }

  auto can_extract_prev_raw_line() -> bool { return get_window_begin() > 0; }

  auto split_line(const std::string& line, const char sep = ' ') const -> std::vector<std::string> {
    std::vector<std::string> ret;

    for (size_t i = 0; i < line.size();) {
      // Take the rest of the line if possible
      uint32_t width = line.size() - i;

      if (i + m_width < line.size()) {
        // Otherwise, take until the last separator if there is one
        int next_space = line.find_last_of(sep, i + m_width - 1);

        if (next_space == -1 || (next_space >= 0 && (unsigned)next_space < i)) {
          // No space before next cut
          // We just simply cut at that point.
          // In extreme cases a word will be seperated
          width = m_width;
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

  auto get_window_begin() -> std::streampos {
    if (m_raw_lines.empty()) {
      return m_anchor;
    }

    return m_raw_lines.front().line.begin_pos;
  }

  auto get_window_end() -> std::streampos {
    if (m_raw_lines.empty()) {
      return m_anchor;
    }

    return m_raw_lines.back().line.end_pos;
  }
};