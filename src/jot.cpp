#include <jot.hpp>

namespace jot {
void init(void) { shrdmm::create("jot/lock", sizeof(std::mutex)); }
void deinit(void) { shrdmm::destroy("jot/lock"); }
} // namespace jot