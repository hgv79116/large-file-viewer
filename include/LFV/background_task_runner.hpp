#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

class BackgroundTaskRunner {
public:
  BackgroundTaskRunner();
  BackgroundTaskRunner(BackgroundTaskRunner&&) = delete;
  BackgroundTaskRunner(const BackgroundTaskRunner&) = delete;

  auto operator=(BackgroundTaskRunner&&) -> BackgroundTaskRunner& = delete;
  auto operator=(const BackgroundTaskRunner&) -> BackgroundTaskRunner& = delete;

  ~BackgroundTaskRunner();

  void loop();

  void quit();

  auto can_run_task() -> bool;

  void run_task(std::function<void()> task);

private:
  mutable std::mutex m_mutex;
  mutable std::condition_variable m_cv;

  std::atomic<bool> m_is_busy;
  std::atomic<bool> m_has_quitted;
  std::function<void()> m_queued_task;

  auto get_queued_task_wait() -> std::function<void()>;
};