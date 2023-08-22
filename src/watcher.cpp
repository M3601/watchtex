#include <watcher.hpp>

#include <jot.hpp>
extern "C" {
#include <sys/inotify.h>
}

watcher_t::watcher_t(void) : events_semaphore(0) {
  this->fd = inotify_init();
  this->running.store(false);
  while (this->events.size()) this->events.pop();
  this->nodes.clear();
}
watcher_t::~watcher_t(void) {
  this->stop();
  close(this->fd);
}
void watcher_t::add(std::filesystem::path path, bool recursive) {
  path = std::filesystem::canonical(path);
  path = std::filesystem::absolute(path);
  jot::debug("watching `{}`", path.string());
  if (path.string().find("node_modules") != std::string::npos) return;
  if (!std::filesystem::exists(path)) {
    jot::warn("watcher_t::add: path `{}` does not exist", path.string());
    return;
  }
  if (recursive && std::filesystem::is_directory(path)) {
    for (auto &&entry : std::filesystem::directory_iterator(path)) {
      if (std::filesystem::is_directory(entry)) this->add(entry, recursive);
    }
  }
  i32 wd = inotify_add_watch(this->fd, path.c_str(), IN_ALL_EVENTS);
  if (wd == -1) {
    jot::warn("watcher_t::add: failed to add path `{}`", path.string());
    return;
  }
  std::lock_guard<std::mutex> lock(this->nodes_mutex);
  this->nodes[wd] = path;
}
void watcher_t::remove(std::filesystem::path path, bool recursive) { die("not implemented"); }
void watcher_t::start(void) {
  if (this->running.exchange(true)) return;
  this->antenna = std::thread([this]() { this->depot(); });
}
void watcher_t::stop(void) {
  if (!this->running.exchange(false)) return;
  this->antenna.join();
}
event_t watcher_t::poll(void) {
  if (!this->running.load()) {
    jot::warn("watcher_t::poll: watcher is not running");
    return { std::filesystem::path(), 0 };
  }
  this->events_semaphore.acquire();
  std::lock_guard<std::mutex> lock(this->events_mutex);
  event_t event = this->events.front();
  this->events.pop();
  return event;
}

void watcher_t::depot(void) {
  static constexpr u64 BUFFER_SIZE = 0x10000;
  byte *buffer                     = new byte[BUFFER_SIZE];
  while (this->running.load()) {
    i64 length = read(this->fd, buffer, BUFFER_SIZE);
    if (length == -1) {
      this->running.store(false);
      die("watcher_t::depot: failed to read from inotify");
    }
    if (length == 0) {
      this->running.store(false);
      die("watcher_t::depot: inotify has been closed");
    }
    i64 offset = 0;
    while (offset < length) {
      struct inotify_event *event = (struct inotify_event *)(buffer + offset);
      this->nodes_mutex.lock();
      std::filesystem::path path = this->nodes[event->wd];
      this->nodes_mutex.unlock();
      if (event->len) { path /= event->name; }
      offset += sizeof(*event) + event->len;
      if (static_cast<u64>(offset) > BUFFER_SIZE) {
        this->running.store(false);
        die("watcher_t::depot: buffer overflow");
      }
      std::lock_guard<std::mutex> lock(this->events_mutex);
      this->events.push(event_t{ path, event->mask });
      this->events_semaphore.release();
    }
  }
  delete[] buffer;
}