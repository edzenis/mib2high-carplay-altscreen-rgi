#ifndef ALT_LRU_SLOTS_H
#define ALT_LRU_SLOTS_H
#include <stdint.h>
#include <stddef.h>
/* Firmware-independent bounded active-slot LRU allocator for route data.
 * Slot payloads remain with the transport adapter, but reusable selection,
 * pinning, versioning and eviction decisions live in the universal core. */
typedef struct alt_lru_slots {
    uint16_t *keys;
    uint32_t *seq;
    uint32_t *version; /* optional */
    uint32_t *counter;
    uint32_t *version_counter; /* optional */
    size_t capacity;
} alt_lru_slots;
/* protection[i] prevents eviction, not updating an existing slot.
 * result: 0 existing slot, 1 free slot, 2 existing slot evicted.
 * Returns -1 if no match and create=0 or invalid state. */
__attribute__((visibility("hidden"))) int alt_core_lru_resolve(alt_lru_slots *slots,
    uint16_t index,int create,const unsigned char *protection,int *result);
#endif
