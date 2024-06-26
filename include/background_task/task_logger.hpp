#include <sstream>
#include <queue>

/* Common logging class for all background tasks */
class TaskLogger {
public:
  TaskLogger() = default;

  TaskLogger(const TaskLogger&) = delete;
  TaskLogger(TaskLogger&&) = delete;
  TaskLogger& operator=(const TaskLogger&) = delete;
  TaskLogger& operator=(TaskLogger&&) = delete;

  ~TaskLogger() = default;

  template <typename T> TaskLogger& operator<<(T value) {
    std::stringstream sstream{};
    sstream << value;
    char chr{};
    std::string cur_message;
    while (sstream >> chr) {
      if (chr == '\n') {
        _message_queue.push(cur_message);
        cur_message.clear();
      } else {
        cur_message.push_back(chr);
      }
    }

    return *this;
  }

  bool hasQueuedMessage() { return !_message_queue.empty(); }

  // Pop the oldest message from the queue and return it
  std::string popMessage() {
    auto oldest = _message_queue.front();
    _message_queue.pop();
    return oldest;
  }

private:
  std::queue<std::string> _message_queue;
};
