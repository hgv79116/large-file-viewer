#include <atomic>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

class BackgroundTaskRunner {
public:
  static BackgroundTaskRunner& get_instance();

  bool can_run_task();

  void run_task(std::function<void(void)>&& task);

  ~BackgroundTaskRunner();
private:
  static BackgroundTaskRunner m_instance;
  mutable std::mutex m_mutex;
  std::atomic<bool> m_is_busy;
  std::atomic<bool> m_has_quitted;
  std::function<void(void)> m_queued_task;
  std::thread m_thread;

  BackgroundTaskRunner();

  void loop();

  std::function<void(void)> get_queued_task();
};