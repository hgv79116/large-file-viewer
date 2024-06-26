#ifndef LFV_SEARCH_RESULT

#define LFV_SEARCH_RESULT

#include <atomic>
#include <mutex>
#include <vector>

enum class BackgroundTaskStatus { NOT_STARTED = 0, ONGOING = 1, FINISHED = 2, ABORTED = 3 };

class SearchResult {
public:
  SearchResult();

  int getNumMatches() const { return static_cast<int>(m_matches.size()); }

  std::streampos getMatch(int index) const;

  void addMatch(std::streampos pos);

  void setCurrentPos(std::streampos pos) { m_current_pos = pos; }

  int64_t getCurrentPos() { return m_current_pos; }

  inline BackgroundTaskStatus getStatus() { return m_status.load(); }

  inline void setStatus(BackgroundTaskStatus status) { m_status = status; }

private:
  std::atomic<BackgroundTaskStatus> m_status;
  std::atomic<int64_t> m_current_pos = 0;
  mutable std::mutex m_mutex;
  std::vector<std::streampos> m_matches;
};

#endif