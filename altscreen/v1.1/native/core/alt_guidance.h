#ifndef ALTSCREEN_GUIDANCE_CORE_H
#define ALTSCREEN_GUIDANCE_CORE_H
#include <stdint.h>
/* Protocol-neutral route evidence and pending-zero policy. */
typedef struct {int state_positive,maneuver_count,maneuver_list,distance_to_maneuver,distance_remaining,destination,current_road;} alt_core_route_evidence;
__attribute__((visibility("hidden"))) int alt_core_has_active_route(const alt_core_route_evidence *e);
__attribute__((visibility("hidden"))) uint64_t alt_core_zero_deadline(uint64_t now,int from_reroute,uint64_t grace_ms);
/* CarPlay route reset timing is a universal navigation policy. */
#define ALT_CORE_RGI_ZERO_GRACE_MS 800

/* Deferred CarPlay route_state=0 suppression belongs to the universal
 * route policy; the K2161 ingress adapter only holds a pending instance. */
typedef struct alt_core_zero_pending {
    uint64_t started_ms;
    uint64_t deadline_ms;
    int from_reroute;
} alt_core_zero_pending;
__attribute__((visibility("hidden"))) void alt_core_zero_start(alt_core_zero_pending*,uint64_t,int,uint64_t);
__attribute__((visibility("hidden"))) int alt_core_zero_cancel(alt_core_zero_pending*);
__attribute__((visibility("hidden"))) int alt_core_zero_due(const alt_core_zero_pending*,int,uint64_t);
#endif
