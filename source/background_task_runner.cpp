#include <LFV/background_task_runner.hpp>
#include <LFV/lfv_exception.hpp>
#include <condition_variable>
#include <iostream>
#include <set>
#include <unordered_set>
#include <utility>

BackgroundTaskRunner::BackgroundTaskRunner() : m_is_busy(false), m_has_quitted(false) {
  // Start the thread
}

void BackgroundTaskRunner::loop() {
  while (!m_has_quitted) {
    auto task = get_queued_task_wait();

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

void BackgroundTaskRunner::quit() { m_has_quitted = true; }

bool BackgroundTaskRunner::can_run_task() { return !m_is_busy; }

void BackgroundTaskRunner::run_task(std::function<void()> task) {
  std::unique_lock<std::mutex> lock(m_mutex);

  if (m_is_busy) {
    throw LFVException("Task requested while runner is busy");
  }

  m_queued_task = std::move(task);

  // Notify the consumer thread
  m_cv.notify_one();
}

std::function<void()> BackgroundTaskRunner::get_queued_task_wait() {
  std::unique_lock<std::mutex> lock(m_mutex);

  // Wait until
  // 1. The runner is flagged to quit or
  // 2. The queued task is not empty
  m_cv.wait(lock, [this] { return m_has_quitted || m_queued_task; });

  auto task = std::function<void()>();

  if (m_queued_task) {
    // Clear the queued task
    task.swap(m_queued_task);
  }

  // In case the quit flag is on, return empty task
  // Otherwise return the queued task
  return task;
}

BackgroundTaskRunner::~BackgroundTaskRunner() {}