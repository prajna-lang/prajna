#pragma once

#include <atomic>
#include <unordered_map>

namespace prajna {

extern std::unordered_map<void *, std::atomic<int64_t>> ptr_count_dict;

inline void RegisterReferenceCount(void *Pointer) { ptr_count_dict[Pointer].store(0); }
inline int64_t GetReferenceCount(void *Pointer) { return ptr_count_dict[Pointer].load(); }
inline void IncrementReferenceCount(void *Pointer) { ++ptr_count_dict[Pointer]; }
inline void DecrementReferenceCount(void *Pointer) { --ptr_count_dict[Pointer]; }

}  // namespace prajna
