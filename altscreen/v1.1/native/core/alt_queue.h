#ifndef ALTSCREEN_PORTABLE_QUEUE_H
#define ALTSCREEN_PORTABLE_QUEUE_H
/* Bounded ring indices. Caller owns locking and payload memory. */
__attribute__((visibility("hidden"))) int alt_core_ring_push(unsigned int *head,unsigned int *count,unsigned int capacity,unsigned int *slot);
__attribute__((visibility("hidden"))) int alt_core_ring_pop(unsigned int *tail,unsigned int *count,unsigned int capacity,unsigned int *slot);
#endif
