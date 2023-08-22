#ifndef RKHASH_HPP
#define RKHASH_HPP

#pragma once

#include <array>
#include <string_view>
#include <types.hpp>

template <u64 G, u64... M>
class hash_t {
private:
  std::array<u64, sizeof...(M)> keys, coeff, inv;

  constexpr inline u64 fast_pow(u64 b, u64 e) const {
    u64 r = 1;
    b %= G;
    while (e) {
      if (e & 1) r = (r * b) % G;
      b = (b * b) % G;
      e >>= 1;
    }
    return r;
  }
  constexpr inline u64 fast_inv(u64 b) const { return fast_pow(b, G - 2ul); }
  constexpr inline void init() {
    u32 i = 0;
    for (auto m : { M... }) {
      keys[i]  = 0;
      coeff[i] = 1;
      inv[i]   = fast_inv(m);
      i++;
    }
    length = 0;
  }

public:
  u64 length;
  constexpr hash_t() { init(); }
  constexpr hash_t(const std::string_view &&s) {
    init();
    for (auto &&c : s) add_right(c);
  }
  constexpr hash_t(const std::string_view &s) {
    init();
    for (auto &&c : s) add_right(c);
  }
  constexpr ~hash_t() {}
  constexpr friend bool operator==(const hash_t &a, const hash_t &b) {
    return a.length == b.length && a.keys == b.keys;
  }
  constexpr friend bool operator!=(const hash_t &a, const hash_t &b) {
    return a.length != b.length || a.keys != b.keys;
  }
  constexpr void add_right(u64 x) {
    u32 i = 0;
    for (auto m : { M... }) {
      keys[i]  = (keys[i] * m + x) % G;
      coeff[i] = (coeff[i] * m) % G;
      i++;
    }
    length++;
  }
  constexpr void add_left(u64 x) {
    u32 i = 0;
    for (auto m : { M... }) {
      keys[i] += (x * coeff[i]) % G;
      keys[i] %= G;
      coeff[i] = (coeff[i] * m) % G;
      i++;
    }
    length++;
  }
  constexpr void remove_right(u64 x) {
    for (u32 i = 0; i < sizeof...(M); i++) {
      keys[i] = (keys[i] + G - x % G) % G;
      keys[i] *= inv[i];
      keys[i] %= G;
      coeff[i] = (coeff[i] * inv[i]) % G;
    }
    length--;
  }
  constexpr void remove_left(u64 x) {
    for (u32 i = 0; i < sizeof...(M); i++) {
      coeff[i] = (coeff[i] * inv[i]) % G;
      auto tmp = (x * coeff[i]) % G;
      keys[i]  = (keys[i] + G - tmp) % G;
    }
    length--;
  }
};

#endif