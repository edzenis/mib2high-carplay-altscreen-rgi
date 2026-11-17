#include "core/alt_trace_ports.h"
#ifndef ALTSCREEN_K2161_TRACE_INPUT_H
#define ALTSCREEN_K2161_TRACE_INPUT_H
/* K2161 DSITracer wire-format parser; outputs normalized values only. */
__attribute__((visibility("hidden"))) int k2161_gauge_from_trace(const char *line,int buffered,int *slot,unsigned int *raw);
__attribute__((visibility("hidden"))) int k2161_zoom_area_from_trace(const char *line,int *slot,int *x,int *y,int *w,int *h);
__attribute__((visibility("hidden"))) int k2161_wheel_from_trace(const char *line,int buffered,int *step);
#endif

__attribute__((visibility("hidden"))) void k2161_trace_classify(const char *,alt_trace_tags_t *);
