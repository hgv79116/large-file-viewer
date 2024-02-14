#include <search_result.hpp>

SearchResult::SearchResult():
  status(BackgroundTaskStatus::NOT_STARTED) {
}

int32_t SearchResult::get_num_matches() const {
  return m_matches.size();
}

std::streampos SearchResult::get_match(int32_t index) const {
  std::unique_lock<std::mutex> lock(m_mutex);

  return m_matches[index];
}

void SearchResult::add_match(std::streampos pos) {
  std::unique_lock<std::mutex> lock(m_mutex);

  m_matches.push_back(pos);
}