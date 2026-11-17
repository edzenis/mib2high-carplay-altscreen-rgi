#include "rgd_lane_resolution.h"
#include <string.h>
int alt_rgi_resolve_linked_lane(rgd_maneuver_t *cache,int count,int destination,
    int (*lookup)(uint16_t,void*),void *context) {
    if(!cache || !lookup || destination<0 || destination>=count)return -1;
    rgd_maneuver_t *man=&cache[destination];
    if(!(man->present&RGD_MAN_LINKED_LANE_INDEX) ||
       (man->present&RGD_MAN_LANE_GUIDANCE))return -1;
    int slot=lookup(man->linked_lane_guidance_index,context);
    if(slot<0 || slot>=count)return -1;
    const rgd_maneuver_t *src=&cache[slot];
    if(!(src->present&RGD_MAN_LANE_GUIDANCE) || src->lane_count==0)return -1;
    man->lane_count=src->lane_count;
    memcpy(man->lanes,src->lanes,sizeof(man->lanes));
    man->present|=RGD_MAN_LANE_GUIDANCE;
    return slot;
}
unsigned alt_rgi_backpropagate_linked_lanes(rgd_maneuver_t *cache,
    const uint16_t *slot_keys,int count,int source) {
    unsigned changed=0;
    if(!cache || !slot_keys || count<=0 || count>32 ||source<0||source>=count)return 0;
    const rgd_maneuver_t *src=&cache[source];
    if(!(src->present&RGD_MAN_LANE_GUIDANCE)||src->lane_count==0||
       slot_keys[source]==0xFFFF)return 0;
    for(int s=0;s<count;s++) {
        if(s==source)continue;
        rgd_maneuver_t *dst=&cache[s];
        if(!(dst->present&RGD_MAN_LINKED_LANE_INDEX) ||
           (dst->present&RGD_MAN_LANE_GUIDANCE) ||
           dst->linked_lane_guidance_index!=slot_keys[source])continue;
        dst->lane_count=src->lane_count;
        memcpy(dst->lanes,src->lanes,sizeof(dst->lanes));
        dst->present|=RGD_MAN_LANE_GUIDANCE;
        changed|=1u<<s;
    }
    return changed;
}
