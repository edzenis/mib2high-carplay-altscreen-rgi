#include "alt_rgi_index.h"
int alt_core_rgi_min_index(const uint16_t *list,unsigned count) {
    if (!list || !count) return -1;
    uint16_t min=list[0];
    for (unsigned i=1;i<count;i++) if(list[i]<min)min=list[i];
    return (int)min;
}
int alt_core_rgi_accept_index(int have_update,int list_present,
        const uint16_t *list,unsigned count,int is_rerouting,uint16_t idx) {
    if(!have_update)return 0;
    if(!list_present || count==0 || is_rerouting)return 1;
    int min=alt_core_rgi_min_index(list,count);
    return min>=0 && (int)idx>=min;
}
int alt_core_rgi_list_contains(const uint16_t *list,unsigned count,int present,uint16_t idx) {
    if(!list || !present)return 0;
    for(unsigned i=0;i<count;i++)if(list[i]==idx)return 1;
    return 0;
}

int alt_core_rgi_lane_should_prune(int list_present,unsigned list_count,int rerouting,
    int min_index,uint16_t candidate,int has_active_lane,uint16_t active_lane) {
    if(!list_present || !list_count || rerouting || min_index<0)return 0;
    if(candidate==0xFFFFu || (int)candidate>=min_index)return 0;
    if(has_active_lane && candidate==active_lane)return 0;
    return 1;
}
