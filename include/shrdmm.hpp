#ifndef SHRDMM_HPP
#define SHRDMM_HPP

#pragma once

#ifndef __linux__
#error "shrdmm.hpp is only available on Linux"
#endif

#include <string_view>
#include <types.hpp>

namespace shrdmm {
void init(void);
void deinit(void);
void create(std::string_view key, u64 size);
void *mount(std::string_view key);
void drop(void *addr);
void destroy(std::string_view key);
} // namespace shrdmm

#endif