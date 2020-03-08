#pragma once
#include <stdlib.h>

#ifdef __APPLE__
inline void* aligned_alloc(size_t alignment, size_t size)
{
    void* t = 0;
    posix_memalign(&t, alignment, size);
    return t;
}

inline void aligned_free(void* mem) {
  free(mem);
}
#endif

#ifdef __linux__
inline void aligned_free(void* mem) {
  free(mem);
}
#endif

#ifdef _WIN32
inline void* aligned_alloc(size_t alignment, size_t size)
{
    return _aligned_malloc(size, alignment);
}

inline void aligned_free(void* mem) {
  _aligned_free(mem);
}
#endif