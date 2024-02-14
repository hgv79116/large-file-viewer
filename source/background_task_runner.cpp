#include <LFV/background_task_runner.hpp>
#include <LFV/lfv_exception.hpp>
#include <iostream>
#include <utility>

BackgroundTaskRunner::BackgroundTaskRunner() : m_is_busy(false), m_has_quitted(false) {
  // Start the thread
  m_thread = std::thread([this] { this->loop(); });
}

auto BackgroundTaskRunner::can_run_task() -> bool { return !m_is_busy; }

void BackgroundTaskRunner::run_task(std::function<void()> task) {
  std::unique_lock<std::mutex> lock(m_mutex);

  if (m_is_busy) {
    throw LFVException("Task requested while runner is busy");
  }

  m_queued_task = std::move(task);
}

auto BackgroundTaskRunner::get_queued_task() -> std::function<void()> {
  std::unique_lock<std::mutex> lock(m_mutex);

  auto task = std::function<void()>();

  if (m_queued_task) {
    // Clear the current function
    task.swap(m_queued_task);
  }

  return task;
}

void BackgroundTaskRunner::loop() {
  std::function<void()> task;
  while (!m_has_quitted) {
    task = get_queued_task();

    if (task) {
      m_is_busy = true;

      try {
        task();
      } catch (std::exception const& e) {
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