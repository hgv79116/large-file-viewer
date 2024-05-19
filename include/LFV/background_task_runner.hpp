#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

class BackgroundTaskRunner {
public:
  BackgroundTaskRunner();
  BackgroundTaskRunner(BackgroundTaskRunner&&) = delete;
  BackgroundTaskRunner(const BackgroundTaskRunner&) = delete;

  BackgroundTaskRunner& operator=(BackgroundTaskRunner&&) = delete;
  BackgroundTaskRunner& operator=(const BackgroundTaskRunner&) = delete;

  ~BackgroundTaskRunner();

  void loop();

  void quit();

  bool can_run_task();

  void run_task(std::function<void()> task);

private:
  mutable std::mutex m_mutex;
  mutable std::condition_variable m_cv;

  std::atomic<bool> m_is_busy;
  std::atomic<bool> m_has_quitted;
  std::function<void()> m_queued_task;

  std::function<void()> get_queued_task_wait();
};