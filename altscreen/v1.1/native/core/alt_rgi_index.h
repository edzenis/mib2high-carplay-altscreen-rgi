#ifndef ALT_CORE_RGI_INDEX_H
#define ALT_CORE_RGI_INDEX_H
#include <stdint.h>
#include <stddef.h>
/* Portable semantic maneuver index policies (Apple iAP2, no firmware state). */
__attribute__((visibility("hidden"))) int alt_core_rgi_min_index(const uint16_t *list,unsigned count);
__attribute__((visibility("hidden"))) int alt_core_rgi_accept_index(int have_update,int list_present,
            const uint16_t *list,unsigned count,int is_rerouting,uint16_t idx);
__attribute__((visibility("hidden"))) int alt_core_rgi_list_contains(const uint16_t *list,unsigned count,int present,uint16_t idx);
/* Semantic pruning decision. Raw lane-cache writes remain in input. */
__attribute__((visibility("hidden"))) int alt_core_rgi_lane_should_prune(
    int list_present,unsigned list_count,int rerouting,int min_index,
    uint16_t candidate,int has_active_lane,uint16_t active_lane);
#endif
