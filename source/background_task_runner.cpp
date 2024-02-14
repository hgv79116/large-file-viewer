#include <background_task_runner.hpp>
#include <lfv_exception.hpp>
#include <iostream>

BackgroundTaskRunner BackgroundTaskRunner::m_instance = BackgroundTaskRunner();

BackgroundTaskRunner& BackgroundTaskRunner::get_instance() { return m_instance; }

BackgroundTaskRunner::BackgroundTaskRunner():
  m_is_busy(false),
  m_has_quitted(false) {
  // Start the thread
  m_thread = std::thread(std::bind(&BackgroundTaskRunner::loop, this));
}

bool BackgroundTaskRunner::can_run_task() { return !m_is_busy; }


void BackgroundTaskRunner::run_task(std::function<void(void)>&& task) {
  std::unique_lock<std::mutex> lock(m_mutex);

  if (m_is_busy) {
    throw LFVException("Task requested while runner is busy");
  }

  m_queued_task = task;
}

std::function<void(void)> BackgroundTaskRunner::get_queued_task() {
  std::unique_lock<std::mutex> lock(m_mutex);

  auto task = std::function<void(void)>();

  if (m_queued_task) {
    // Clear the current function
    task.swap(m_queued_task);
  }

  return task;
}

void BackgroundTaskRunner::loop() {
  std::function<void(void)> task;
  while (!m_has_quitted) {
    task = get_queued_task();

    if (task) {
      m_is_busy = true;

      try {
        task();
      } catch (std::exception e) {
        std::cerr << e.what() << std::endl;
        exit(0); 
      }

      m_is_busy = false;
    }
  }
}

BackgroundTaskRunner::~BackgroundTaskRunner() {
  m_has_quitted = true;

  m_thread.join();
}