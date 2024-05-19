#include <LFV/search_result.hpp>
#include <ios>
#include <mutex>

SearchResult::SearchResult() : m_status(BackgroundTaskStatus::NOT_STARTED) {}

int SearchResult::get_num_matches() const { return static_cast<int>(m_matches.size()); }

std::streampos SearchResult::get_match(int index) const {
  std::scoped_lock<std::mutex> lock(m_mutex);

  return m_matches[index];
}

void SearchResult::add_match(std::streampos pos) {
  const std::scoped_lock<std::mutex> lock(m_mutex);

  m_matches.push_back(pos);
}