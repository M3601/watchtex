#ifndef JOT_HPP
#define JOT_HPP

#pragma once

#include <cstdlib>
#include <fmt/color.h>
#include <fmt/format.h>
#include <mutex>
#include <shrdmm.hpp>
#include <string_view>
#include <types.hpp>

// TODO: add lock to prevent interleaving of messages
namespace jot {
void init(void);
void deinit(void);
template <typename... T>
void debug(fmt::format_string<T...> message, T &&...args) {
#ifdef DEBUG
  static std::mutex *mtx = nullptr;
  if (mtx == nullptr) [[unlikely]] {
    auto base = shrdmm::mount("jot/lock");
    mtx       = new (base) std::mutex();
  }
  std::lock_guard<std::mutex> lock(*mtx);
  static constexpr auto style  = fmt::fg(fmt::rgb(0x9e9e9e));
  static constexpr auto prefix = fmt::styled("DEBUG", style);
  fmt::print(stderr, "[{}] {}\n", prefix, fmt::format(message, std::forward<T>(args)...));
#endif
}
template <typename... T>
void info(fmt::format_string<T...> message, T &&...args) {
  static std::mutex *mtx = nullptr;
  if (mtx == nullptr) [[unlikely]] {
    auto base = shrdmm::mount("jot/lock");
    mtx       = new (base) std::mutex();
  }
  std::lock_guard<std::mutex> lock(*mtx);
  static constexpr auto style  = fmt::fg(fmt::rgb(0x4fc3f7));
  static constexpr auto prefix = fmt::styled("INFO", style);
  fmt::print(stderr, "[{}] {}\n", prefix, fmt::format(message, std::forward<T>(args)...));
}
template <typename... T>
void warn(fmt::format_string<T...> message, T &&...args) {
  static std::mutex *mtx = nullptr;
  if (mtx == nullptr) [[unlikely]] {
    auto base = shrdmm::mount("jot/lock");
    mtx       = new (base) std::mutex();
  }
  std::lock_guard<std::mutex> lock(*mtx);
  static constexpr auto style  = fmt::fg(fmt::rgb(0xffa000)) | fmt::emphasis::bold;
  static constexpr auto prefix = fmt::styled("WARN", style);
  fmt::print(stderr, "[{}] {}\n", prefix, fmt::format(message, std::forward<T>(args)...));
}
template <typename... T>
void error(fmt::format_string<T...> message, T &&...args) {
  static std::mutex *mtx = nullptr;
  if (mtx == nullptr) [[unlikely]] {
    auto base = shrdmm::mount("jot/lock");
    mtx       = new (base) std::mutex();
  }
  std::lock_guard<std::mutex> lock(*mtx);
  static constexpr auto style  = fmt::fg(fmt::rgb(0xe53935)) | fmt::emphasis::bold | fmt::emphasis::underline;
  static constexpr auto prefix = fmt::styled("ERROR", style);
  fmt::print(stderr, "[{}] {}\n", prefix, fmt::format(message, std::forward<T>(args)...));
}
template <typename... T>
void fatal(fmt::format_string<T...> message, T &&...args) {
  static std::mutex *mtx = nullptr;
  if (mtx == nullptr) [[unlikely]] {
    auto base = shrdmm::mount("jot/lock");
    mtx       = new (base) std::mutex();
  }
  std::lock_guard<std::mutex> lock(*mtx);
  static constexpr auto style  = fmt::fg(fmt::rgb(0x6a1b9a)) | fmt::emphasis::bold | fmt::emphasis::underline;
  static constexpr auto prefix = fmt::styled("FATAL", style);
  fmt::print(stderr, "[{}] {}\n", prefix, fmt::format(message, std::forward<T>(args)...));
}
} // namespace jot

#define die(...)                                                                                                       \
  do {                                                                                                                 \
    jot::fatal(__VA_ARGS__);                                                                                           \
    std::exit(1);                                                                                                      \
  } while (0)

#endif