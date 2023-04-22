#pragma once

#include <atomic>
#include <unordered_map>

namespace prajna {

extern std::unordered_map<void *, std::atomic<int64_t>> ptr_count_dict;

inline void RegisterReferenceCount(void *ptr) { ptr_count_dict[ptr].store(0); }
inline int64_t GetReferenceCount(void *ptr) { return ptr_count_dict[ptr].load(); }
inline void IncrementReferenceCount(void *ptr) { ++ptr_count_dict[ptr]; }
inline void DecrementReferenceCount(void *ptr) { --ptr_count_dict[ptr]; }

}  // namespace prajna
