#ifndef LFV_SEARCH_RESULT

#define LFV_SEARCH_RESULT

#include <atomic>
#include <mutex>
#include <vector>

enum class BackgroundTaskStatus { NOT_STARTED = 0, ONGOING = 1, FINISHED = 2, ABORTED = 3 };

class SearchResult {
public:
  SearchResult();

  auto get_num_matches() const -> int;

  auto get_match(int index) const -> std::streampos;

  void add_match(std::streampos pos);

  void set_current_pos(std::streampos pos) { m_current_pos = pos; }

  auto get_current_pos() -> int64_t { return m_current_pos; }

  inline auto get_status() -> BackgroundTaskStatus { return m_status.load(); }

  inline void set_status(BackgroundTaskStatus status) { m_status = status; }

private:
  std::atomic<BackgroundTaskStatus> m_status;
  std::atomic<int64_t> m_current_pos = 0;
  mutable std::mutex m_mutex;
  std::vector<std::streampos> m_matches;
};

#endif