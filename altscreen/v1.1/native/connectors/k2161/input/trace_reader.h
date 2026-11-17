#ifndef K2161_TRACE_READER_H
#define K2161_TRACE_READER_H
#include <stdint.h>
typedef void (*k2161_trace_event_callback)(const char *line,int buffered);
typedef void (*k2161_trace_metrics_callback)(uint64_t*,uint64_t*,uint64_t*,uint64_t*);
__attribute__((visibility("hidden"))) void k2161_start_trace_readers(k2161_trace_event_callback callback,k2161_trace_metrics_callback metrics);
#endif
