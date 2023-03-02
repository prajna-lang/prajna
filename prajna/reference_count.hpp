#pragma once

#include <atomic>
#include <map>

namespace prajna {

extern std::map<void *, std::atomic<int64_t>> ptr_count_dict;

inline void registerReferenceCount(void *ptr) { ptr_count_dict[ptr].store(0); }
inline int64_t getReferenceCount(void *ptr) { return ptr_count_dict[ptr].load(); }
inline void incrementReferenceCount(void *ptr) { ++ptr_count_dict[ptr]; }
inline void decrementReferenceCount(void *ptr) { --ptr_count_dict[ptr]; }

}  // namespace prajna
