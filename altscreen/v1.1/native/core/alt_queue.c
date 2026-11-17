#include "alt_queue.h"
int alt_core_ring_push(unsigned int *head,unsigned int *count,unsigned int capacity,unsigned int *slot) {
    if (!head || !count || !slot || !capacity || *head>=capacity || *count>=capacity) return 0;
    *slot=*head;*head=(*head+1U)%capacity;++*count;return 1;
}
int alt_core_ring_pop(unsigned int *tail,unsigned int *count,unsigned int capacity,unsigned int *slot) {
    if (!tail || !count || !slot || !capacity || *tail>=capacity || !*count) return 0;
    *slot=*tail;*tail=(*tail+1U)%capacity;--*count;return 1;
}
