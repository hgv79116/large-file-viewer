/* DEPRECATED

#include <LFV/search_result.hpp>
#include <ios>
#include <mutex>

SearchResult::SearchResult() : m_status(BackgroundTaskStatus::NOT_STARTED) {}

std::streampos SearchResult::getMatch(int index) const {
  std::scoped_lock<std::mutex> lock(m_mutex);

  return m_matches[index];
}

void SearchResult::addMatch(std::streampos pos) {
  const std::scoped_lock<std::mutex> lock(m_mutex);

  m_matches.push_back(pos);
}

*/