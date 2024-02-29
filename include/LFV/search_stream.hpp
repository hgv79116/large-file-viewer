#include <LFV/lfv_exception.hpp>
#include <LFV/search_result.hpp>
#include <array>
#include <fstream>
#include <memory>
#include <string>

// BMH algorithm

// NOLINTBEGIN
void search_in_stream(std::ifstream&& in, const std::string& pattern_str, std::streampos begin,
                      std::streampos end, int32_t match_limit, std::shared_ptr<SearchResult> result,
                      std::shared_ptr<std::atomic<bool>> aborted) {
  constexpr int MAX_ALPHABET = 1 << 8;
  constexpr int MAX_PAT_LEN = 1 << 8;

  const size_t pat_len = pattern_str.size();

  if (pat_len > MAX_PAT_LEN) {
    throw LFVException("Pattern length exceeded max pattern length");
  }

  // Copy to array on stack to speed up
  std::array<int, MAX_PAT_LEN> pattern = {};
  for (size_t i = 0; i < pat_len; i++) {
    pattern[i] = static_cast<unsigned char>(pattern_str[i]);
  }

  // Calculate BMH table
  std::array<int, MAX_ALPHABET> table = {};
  for (size_t i = 0; i < MAX_ALPHABET; i++) {
    table[i] = static_cast<int>(pat_len);
  }

  for (size_t i = 0; i + 2 <= pat_len; i++) {
    int ascii = pattern[i];

    table[ascii] = static_cast<int>(pat_len - 1 - i);
  }

  // Consistent state: buffer always contain file content from in.tellg - patlen to in.tellg - 1
  // buffer_index always points to the starting point
  in.seekg(begin);

  std::array<int, MAX_PAT_LEN> buffer = {};
  size_t buffer_index = 0;

  for (size_t k = 0; k < pat_len; k++) {
    buffer[k] = in.get();
  }

  constexpr int32_t HEAVY_CYCLE = 1000;
  // Run string matching until
  // - match limit exceeded OR
  // - reaches EOF
  int count_match = 0;
  int update_countdown = HEAVY_CYCLE;

  result->set_status(BackgroundTaskStatus::ONGOING);

  while (in.tellg() < end && count_match < match_limit) {
    // We update progress and check exit condition
    // every cycle/whenever a match is found to avoid overhead.
    update_countdown--;
    if (update_countdown == 0) {
      result->set_current_pos(in.tellg());
      update_countdown = HEAVY_CYCLE;
      if (*aborted) {
        result->set_status(BackgroundTaskStatus::ABORTED);

        // Exit as we have handled all cleaning up.
        return;
      }
    }

    bool same = true;
    for (size_t j = 0, k = buffer_index; same && j < pat_len; j++, k++) {
      if (k >= pat_len) {
        k -= pat_len;
      }

      same &= pattern[j] == buffer[k];
    }

    int forward_steps = 1;

    if (same) {
      result->add_match(in.tellg() - (std::streampos)pat_len);
      count_match++;

      forward_steps = std::min(1, (int)(end - in.tellg()));
    } else {
      int last_char = buffer_index == 0 ? buffer[pat_len - 1] : buffer[buffer_index - 1];
      forward_steps = std::min(table[last_char], (int)(end - in.tellg()));
    }

    if (forward_steps == 0) {
      break;
    }

    // Read some amount forward
    for (int k = 0; k < forward_steps; k++) {
      buffer[buffer_index] = in.get();
      buffer_index++;
      if (buffer_index == pat_len) {
        buffer_index = 0;
      }
    }
  }

  result->set_status(BackgroundTaskStatus::FINISHED);
}
// NOLINTEND
