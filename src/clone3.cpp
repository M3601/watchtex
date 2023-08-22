#include <clone3.hpp>

extern "C" {
#include <sys/mman.h>
}
#include <jot.hpp>

namespace proc_stack {
void *create(size_t size) {
  void *stack = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
  if (stack == MAP_FAILED) {
    jot::error("mmap failed: {}", strerror(errno));
    return nullptr;
  }
  return stack;
}
void release(void *stack, size_t size) {
  if (munmap(stack, size) == -1) { jot::error("munmap failed: {}", strerror(errno)); }
}
} // namespace proc_stack
