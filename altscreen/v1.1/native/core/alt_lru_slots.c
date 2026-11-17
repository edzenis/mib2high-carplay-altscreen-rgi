#include "alt_lru_slots.h"
#include <limits.h>
int alt_core_lru_resolve(alt_lru_slots *slots,uint16_t index,int create,
                         const unsigned char *protection,int *result) {
    size_t i, victim=0;
    uint32_t best;
    if(result)*result=0;
    if(!slots || !slots->keys || !slots->seq || !slots->counter ||
       slots->capacity==0 || slots->capacity>(size_t)INT_MAX) return -1;
    for(i=0;i<slots->capacity;i++) {
        if(slots->keys[i]==index) {
            slots->seq[i]=++*slots->counter;
            return (int)i;
        }
    }
    if(!create)return -1;
    for(i=0;i<slots->capacity;i++) {
        if(slots->keys[i]==0xFFFFu) {
            slots->keys[i]=index;
            slots->seq[i]=++*slots->counter;
            if(slots->version && slots->version_counter)
                slots->version[i]=++*slots->version_counter;
            if(result)*result=1;
            return (int)i;
        }
    }
    best=UINT32_MAX;victim=slots->capacity;
    for(i=0;i<slots->capacity;i++) {
        if(protection && protection[i])continue;
        if(slots->seq[i]<best) {best=slots->seq[i];victim=i;}
    }
    if(victim==slots->capacity) {
        best=UINT32_MAX;
        for(i=0;i<slots->capacity;i++) {
            if(slots->seq[i]<best) {best=slots->seq[i];victim=i;}
        }
    }
    if(victim==slots->capacity) return -1;
    slots->keys[victim]=index;
    slots->seq[victim]=++*slots->counter;
    if(slots->version && slots->version_counter)
        slots->version[victim]=++*slots->version_counter;
    if(result)*result=2;
    return (int)victim;
}
