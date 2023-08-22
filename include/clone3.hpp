#ifndef CLONE3_HPP
#define CLONE3_HPP

#pragma once

#ifndef __linux__
#error "clone3.hpp is only available on Linux"
#endif

#ifndef __x86_64__
#error "clone3.hpp is only available on x86_64"
#endif

extern "C" {
#include <linux/sched.h>
}
#include <cstdlib>
#include <types.hpp>

extern "C" {
extern int clone3(struct clone_args *args, size_t size, int (*fn)(void *), void *arg);
}

namespace proc_stack {
void *create(size_t size);
void release(void *stack, size_t size);
} // namespace proc_stack

#endif