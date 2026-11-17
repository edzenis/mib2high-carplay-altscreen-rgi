#ifndef ALT_CORE_TRACE_PORTS_H
#define ALT_CORE_TRACE_PORTS_H
#include <stdint.h>
/* A common input-decoder port: each firmware adapter supplies implementations.
 * Outputs see normalized gauge/zoom/wheel values; no DSITracer or BAP API. */
typedef void (*alt_core_trace_listener)(const char *line,int buffered);
typedef void (*alt_core_trace_metrics)(uint64_t*,uint64_t*,uint64_t*,uint64_t*);
typedef struct alt_trace_tags_t {
    int primary;
    int diag_class; /* -1=not a sample */
    int has_keypanel;
    int has_viewport;
    int has_zoomarea;
    int has_encoder;
    int matches_zoom_encoder;
} alt_trace_tags_t;
/* Target-independent normalized trace event. `raw` is diagnostics-only and
 * borrowed during the callback; output logic must not parse its contents. */
typedef struct alt_core_trace_event {
  const char *raw;
  int buffered;
  alt_trace_tags_t tags;
  int gauge_valid; int gauge_slot; unsigned int gauge_raw;
  int zoom_valid; int zoom_slot; int zoom_x,zoom_y,zoom_w,zoom_h;
  int wheel_valid; int wheel_step;
} alt_core_trace_event;
typedef void (*alt_core_trace_event_listener)(const alt_core_trace_event*);
typedef struct alt_core_trace_input_port {
  void (*start)(alt_core_trace_listener cb,alt_core_trace_metrics metrics);
  void (*classify)(const char*,alt_trace_tags_t*);
  int (*gauge)(const char*,int,int*,unsigned int*);
  int (*zoom_area)(const char*,int*,int*,int*,int*,int*);
  int (*wheel)(const char*,int,int*);
} alt_core_trace_input_port;
__attribute__((visibility("hidden"))) void alt_core_bind_trace_input(const alt_core_trace_input_port*);
__attribute__((visibility("hidden"))) void alt_core_trace_classify(const char*,alt_trace_tags_t*);
__attribute__((visibility("hidden"))) void alt_core_start_trace(alt_core_trace_event_listener,alt_core_trace_metrics);
__attribute__((visibility("hidden"))) int alt_core_trace_gauge(const char*,int,int*,unsigned int*);
__attribute__((visibility("hidden"))) int alt_core_trace_zoom(const char*,int*,int*,int*,int*,int*);
__attribute__((visibility("hidden"))) int alt_core_trace_wheel(const char*,int,int*);
#endif
