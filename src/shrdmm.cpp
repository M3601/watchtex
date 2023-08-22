#include <shrdmm.hpp>

extern "C" {
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
}
#include <fmt/format.h>
#include <map>
#include <set>
#include <string>

static std::map<std::string, u64> *sizes    = nullptr;
static std::map<void *, std::string> *addrs = nullptr;

static std::string replace(std::string str, char from, char to) {
  for (auto &c : str)
    if (c == from) c = to;
  return str;
}
static auto keygen(std::string_view key) { return fmt::format("/watchtex-{}", replace(std::string(key), '/', '-')); }

namespace shrdmm {
void init(void) {
  { // sizes
    constexpr auto size = sizeof(*sizes);
    if (sizes != nullptr) return;
    const auto reference = keygen("shrdmm/sizes");

    auto fd = shm_open(reference.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
      fmt::print(stderr, "shrdmm::init: shm_open failed ({})\n", strerror(errno));
      return;
    }
    if (ftruncate(fd, size) == -1) {
      fmt::print(stderr, "shrdmm::init: ftruncate failed ({})\n", strerror(errno));
      return;
    }
    void *addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
      fmt::print(stderr, "shrdmm::init: mmap failed ({})\n", strerror(errno));
      return;
    }
    sizes = new (addr) std::map<std::string, u64>();
  }
  { // addrs
    constexpr auto size = sizeof(*addrs);
    if (addrs != nullptr) return;
    const auto reference = keygen("shrdmm/addrs");

    auto fd = shm_open(reference.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
      fmt::print(stderr, "shrdmm::init: shm_open failed ({})\n", strerror(errno));
      return;
    }
    if (ftruncate(fd, size) == -1) {
      fmt::print(stderr, "shrdmm::init: ftruncate failed ({})\n", strerror(errno));
      return;
    }
    void *addr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
      fmt::print(stderr, "shrdmm::init: mmap failed ({})\n", strerror(errno));
      return;
    }
    addrs = new (addr) std::map<void *, std::string>();
  }
}
void deinit(void) {
  {
    const auto reference = keygen("shrdmm/sizes");
    if (shm_unlink(reference.c_str()) == -1) {
      fmt::print(stderr, "shrdmm::destroy: shm_unlink failed ({})\n", strerror(errno));
      return;
    }
  }
  {
    const auto reference = keygen("shrdmm/addrs");
    if (shm_unlink(reference.c_str()) == -1) {
      fmt::print(stderr, "shrdmm::destroy: shm_unlink failed ({})\n", strerror(errno));
      return;
    }
  }
}

void create(std::string_view key, u64 size) {
  if (key.length() == 0) {
    fmt::println(stderr, "shrdmm::new: key must not be empty");
    return;
  }
  const auto reference = keygen(key);

  if (sizes->contains(reference)) {
    fmt::println(stderr, "shrdmm::new: key already exists");
    return;
  }
  auto fd = shm_open(reference.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0666);
  if (fd == -1) {
    fmt::print(stderr, "shrdmm::new: shm_open failed ({})\n", strerror(errno));
    return;
  }
  if (ftruncate(fd, size) == -1) {
    fmt::print(stderr, "shrdmm::new: ftruncate failed ({})\n", strerror(errno));
    return;
  }
  sizes->insert({ reference, size });
}
void *mount(std::string_view key) {
  if (key.length() == 0) {
    fmt::println(stderr, "shrdmm::mount: key must not be empty");
    return nullptr;
  }
  const auto reference = keygen(key);

  auto fd = shm_open(reference.c_str(), O_RDWR, 0666);
  if (fd == -1) {
    fmt::print(stderr, "shrdmm::mount: shm_open failed ({})\n", strerror(errno));
    return nullptr;
  }
  void *addr = mmap(nullptr, sizes->at(reference), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == MAP_FAILED) {
    fmt::print(stderr, "shrdmm::mount: mmap failed ({})\n", strerror(errno));
    return nullptr;
  }
  addrs->insert({ addr, reference });
  return addr;
}
void drop(void *addr) {
  if (addr == nullptr) {
    fmt::println(stderr, "shrdmm::drop: addr must not be null");
    return;
  }
  if (!addrs->contains(addr)) {
    fmt::println(stderr, "shrdmm::drop: addr must be mounted");
    return;
  }
  const auto reference = addrs->at(addr);

  if (munmap(addr, sizes->at(reference)) == -1) {
    fmt::print(stderr, "shrdmm::drop: munmap failed ({})\n", strerror(errno));
    return;
  }
  addrs->erase(addr);
}
void destroy(std::string_view key) {
  if (key.length() == 0) {
    fmt::println(stderr, "shrdmm::destroy: key must not be empty");
    return;
  }
  const auto reference = keygen(key);

  for (auto &[addr, ref] : *addrs) {
    if (ref == reference) drop(addr);
  }
  if (shm_unlink(reference.c_str()) == -1) {
    fmt::print(stderr, "shrdmm::destroy: shm_unlink failed ({})\n", strerror(errno));
    return;
  }
  sizes->erase(reference);
}
} // namespace shrdmm