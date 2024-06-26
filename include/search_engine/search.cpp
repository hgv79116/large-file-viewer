#include "search.hpp"
#include "LFV/lfv_exception.hpp"
#include <array>
#include <iostream>
#include <string>

// value must be in range [0, mod)
template<typename V, typename M>
inline void modularIncrement(V& value, M mod) {
  if (value == mod - 1) {
    value = 0;
  }
  else {
    value++;
  }
}

// value must be in range [0, mod)
template<typename V, typename M>
inline void modularDecrement(V& value, M mod) {
  if (value == 0) {
    value = mod - 1;
  }
  else {
    value--;
  }
}

class CircularBuffer {
public:
  CircularBuffer(size_t len) : _buffer(len), _len{len} {};

  int& at(size_t index) {
    assert(index < _len && index >= 0); // TO BE REMOVED
    return (*this)[index];
  }

  int& operator[] (size_t index) {
    size_t pos = _start + index;
    if (pos >= _len) pos -= _len;
    return _buffer[pos];
  }

  // Pops the first value and adds new_val to the end
  // Returns the old front value
  int shift(int new_val) {
    int old_val = _buffer[_start];
    _buffer[_start] = new_val;
    modularIncrement(_start, _len);
    return old_val;
  }

  bool operator==(std::string_view view) {
    if (_len != view.size()) return false;
    for (size_t index = 0; index < _len; index++) {
      if ((*this)[index] != view[index]) return false;
    }
    return true;
  }

  friend std::ostream& operator <<(std::ostream& out, CircularBuffer& buf) {
    out << "Buffer{";
    for (size_t index = 0; index < buf._len; index++) {
      out << buf[index];
      if (index + 1 < buf._len) out << ',';
    }
    out << '}';
    return out;
  }

private:
  std::vector<int> _buffer;
  size_t _len;
  size_t _start = 0;
};

void Search::start() {
  constexpr int kMaxAlphabet = 1 << 8;
  constexpr int kMaxPatLen = 1 << 8;
  constexpr int kHeavyCycle = 1000;

  const std::string& pattern = _config.pattern;
  const int pat_len = static_cast<int>(pattern.size());

  if (pat_len > kMaxPatLen) {
    throw LFVException{"Pattern length exceeded the limit of " + std::to_string(kMaxPatLen)};
  }

  // Initialise BMH jump_table
  std::vector<int> jump_table (kMaxAlphabet, pat_len);
  for (int index = 0; index + 2 <= pat_len; index++) {
    int ascii = pattern[index];
    jump_table[ascii] = pat_len - 1 - index;
  }

  CircularBuffer buffer(pat_len);

  // Start search
  // Clear fail bits to avoid weird stream behaviours
  _stream_ptr->clear();
  _stream_ptr->seekg(_config.start);
  _status = BackgroundTaskStatus::Running;

  // Read the first pat_len - 1 characters
  for (int index = 0;
       index < pat_len - 1 && _stream_ptr->tellg() != _config.end && _stream_ptr->peek() != EOF;
       index++) {
    buffer[index] = _stream_ptr->get();
  }

  int update_countdown = kHeavyCycle;
  int forward_steps = 1;
  // Cannot use stream::eof() as it only return false when we try to read past the end
  // E.g it eof bit is only set when we already consumed EOF
  while (_stream_ptr->tellg() != _config.end && _stream_ptr->peek() != EOF && _matches.size() < _config.limit) {
    while (forward_steps > 0 && _stream_ptr->tellg() != _config.end && _stream_ptr->peek() != EOF) {
      buffer.shift(_stream_ptr->get());
      forward_steps--;
    }

    bool match = buffer == pattern;
    if (match) {
      addMatch(_stream_ptr->tellg() - (std::streampos)pat_len);
      forward_steps = 1;
    } else {
      int last_char = buffer[pat_len - 1];
      forward_steps = jump_table[last_char];
    }

    // Update progress and check exit condition
    // every heavy cycle/whenever a match is found to avoid overhead.
    update_countdown--;
    if (update_countdown == 0 || match) {
      if (_cancel_requested) {
        cancel();
        return;
      }

      setSearchProgress(_stream_ptr->tellg(), false);
      update_countdown = kHeavyCycle;
    }
  }

  _status = BackgroundTaskStatus::Finished;

  announce({SearchEventType::Finished, clock::now()});
}

void Search::addMatch(std::streampos pos) {
  _matches.push_back(pos);

  announce({SearchEventType::FoundNew, clock::now()});
}

void Search::setSearchProgress(std::streampos cur_pos, bool should_announce) {
  _search_progress = cur_pos;

  if (should_announce) {
    announce({SearchEventType::ProgressUpdate, clock::now()});
  }
}

void Search::cancel() {
  _status = BackgroundTaskStatus::Cancelled;

  announce({SearchEventType::Cancelled, clock::now()});
}

void Search::announce(SearchEvent event) {
  for (auto listener_ptr: _listener_ptrs) {
    listener_ptr->onEvent(event);
  }
}