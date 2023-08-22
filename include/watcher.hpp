#ifndef WATCHER_HPP
#define WATCHER_HPP

#pragma once

#ifndef __linux__
#error "watcher.hpp is only available on Linux"
#endif

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <queue>
#include <semaphore>
#include <thread>
#include <types.hpp>

struct event_t {
  std::filesystem::path path;
  u32 mask;
};

class watcher_t {
private:
  i32 fd;
  std::atomic<bool> running;
  std::thread antenna;
  std::map<i32, std::filesystem::path> nodes;
  std::queue<event_t> events;
  std::mutex events_mutex, nodes_mutex;
  std::counting_semaphore<0x7fffffff> events_semaphore;

  void depot(void);

public:
  watcher_t(void);
  ~watcher_t(void);
  void add(std::filesystem::path path, bool recursive = true);
  void remove(std::filesystem::path path, bool recursive = true);
  void start(void);
  void stop(void);
  event_t poll(void);
};

#endif