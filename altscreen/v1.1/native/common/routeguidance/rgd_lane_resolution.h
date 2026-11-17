#ifndef RGD_LANE_RESOLUTION_H
#define RGD_LANE_RESOLUTION_H
#include "rgd_tlv.h"
#include <stdint.h>
/* Shared CarPlay linked-lane semantics; caller retains transport mapping and
 * bounded cache memory. Lookup callback preserves connector's LRU touch policy. */
__attribute__((visibility("hidden"))) int alt_rgi_resolve_linked_lane(
    rgd_maneuver_t *cache,int count,int destination,
    int (*lookup)(uint16_t index,void *context),void *context);
__attribute__((visibility("hidden"))) unsigned alt_rgi_backpropagate_linked_lanes(
    rgd_maneuver_t *cache,const uint16_t *slot_keys,int count,int source);
#endif
