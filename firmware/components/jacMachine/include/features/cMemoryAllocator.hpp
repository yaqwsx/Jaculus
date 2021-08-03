#pragma once

namespace jac {

template < typename Self >
class CMemoryAllocator {
public:
    MACHINE_FEATURE_SELF();

    struct Configuration {};

    void initialize() {}

    void onEventLoop() {}

    static void *allocateMemory( void *udata, duk_size_t size ) {
        // printf("Allocating %d, free: %d bytes, %d bytes\n", size, esp_get_free_heap_size(), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
        void *p = malloc( size );
        // printf("  %p\n", p);
        return p;
    }

    static void *reallocateMemory( void *udata, void *ptr, duk_size_t size ) {
        // printf("Reallocating %d, free: %d bytes\n", size, esp_get_free_heap_size());
        void *p = realloc( ptr, size );
        // printf("  %p\n", p);
        return p;
    }

    static void freeMemory( void *udata, void *ptr ) {
        free( ptr );
    }
};

} // namespace jac