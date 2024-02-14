#ifndef LFV_SEARCH_RESULT

#define LFV_SEARCH_RESULT


#include <vector>
#include <mutex>

enum class BackgroundTaskStatus { NOT_STARTED = 0, ONGOING = 1, FINISHED = 2, ABORTED = 3 };

class SearchResult {
public:
  SearchResult();

  int32_t get_num_matches() const;

  std::streampos get_match(int index) const;

  void add_match(std::streampos pos);

  void set_current_pos(std::streampos pos) { m_current_pos = pos;  }

  int64_t get_current_pos() { return m_current_pos;  }

  std::atomic<BackgroundTaskStatus> status;

private:
  std::atomic<int64_t> m_current_pos;
  mutable std::mutex m_mutex;
  std::vector<std::streampos> m_matches;
};

#endif