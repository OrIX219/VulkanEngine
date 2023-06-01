#pragma once

#include <cstdint>
#include <string_view>

constexpr uint32_t fnv1a_32(char const* s, size_t count) {
  return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) *
         16777619u;
}

constexpr size_t ConstStrlen(const char* s) {
  size_t size = 0;
  while (s[size]) ++size;
  return size;
}

struct StringHash {
  uint32_t computed_hash;

  constexpr StringHash(uint32_t hash) noexcept : computed_hash(hash) {}

  constexpr StringHash(const char* s) noexcept : computed_hash(0) {
    computed_hash = fnv1a_32(s, ConstStrlen(s));
  }

  constexpr StringHash(const char* s, size_t count) noexcept
      : computed_hash(0) {
    computed_hash = fnv1a_32(s, count);
  }

  constexpr StringHash(std::string_view s) noexcept : computed_hash(0) {
    computed_hash = fnv1a_32(s.data(), s.size());
  }

  StringHash(const StringHash& other) = default;

  constexpr operator uint32_t() noexcept { return computed_hash; }
};