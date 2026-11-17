#include "alt_guidance.h"
int alt_core_has_active_route(const alt_core_route_evidence *e) {
    return e && (e->state_positive || e->maneuver_count || e->maneuver_list || e->distance_to_maneuver || e->distance_remaining || e->destination || e->current_road);
}
uint64_t alt_core_zero_deadline(uint64_t now,int from_reroute,uint64_t grace_ms) {
    return from_reroute?0ULL:now+grace_ms;
}

void alt_core_zero_start(alt_core_zero_pending *s,uint64_t now,int reroute,uint64_t grace) {
    if(!s || s->started_ms)return;
    s->started_ms=now;
    s->from_reroute=!!reroute;
    s->deadline_ms=alt_core_zero_deadline(now,reroute,grace);
}
int alt_core_zero_cancel(alt_core_zero_pending *s) {
    if(!s)return 0;
    int was_pending=s->started_ms!=0;
    s->started_ms=0;
    s->deadline_ms=0;
    s->from_reroute=0;
    return was_pending;
}
int alt_core_zero_due(const alt_core_zero_pending *s,int emitted_is_zero,uint64_t now) {
    return s && s->started_ms && !emitted_is_zero &&
      !s->from_reroute && s->deadline_ms!=0 && now>=s->deadline_ms;
}
