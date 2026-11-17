#include "alt_trace_ports.h"
#include <string.h>
static alt_core_trace_input_port bound;
void alt_core_bind_trace_input(const alt_core_trace_input_port*p){if(p)memcpy(&bound,p,sizeof(bound));}
static alt_core_trace_event_listener sink;
static void normalize_trace(const char *raw,int buffered) {
    alt_core_trace_event e;
    memset(&e,0,sizeof(e));
    e.raw=raw; e.buffered=buffered; e.tags.diag_class=-1;
    if(bound.classify) bound.classify(raw,&e.tags);
    if(bound.gauge) e.gauge_valid=bound.gauge(raw,buffered,&e.gauge_slot,&e.gauge_raw);
    /* Parse optional values before dispatch, but output retains existing
     * precedence rules (BAP54 > zoom-area heuristic, buffered drop, etc.). */
    if(e.tags.has_zoomarea && bound.zoom_area)
       e.zoom_valid=bound.zoom_area(raw,&e.zoom_slot,&e.zoom_x,&e.zoom_y,&e.zoom_w,&e.zoom_h);
    if(e.tags.has_encoder && bound.wheel)
       e.wheel_valid=bound.wheel(raw,buffered,&e.wheel_step);
    if(sink) sink(&e);
}
void alt_core_start_trace(alt_core_trace_event_listener l,alt_core_trace_metrics m) {
    sink=l;
    if(bound.start) bound.start(normalize_trace,m);
}
int alt_core_trace_gauge(const char*s,int b,int*v,unsigned int*r){return bound.gauge?bound.gauge(s,b,v,r):0;}
int alt_core_trace_zoom(const char*s,int*sl,int*x,int*y,int*w,int*h){return bound.zoom_area?bound.zoom_area(s,sl,x,y,w,h):0;}
int alt_core_trace_wheel(const char*s,int b,int*v){return bound.wheel?bound.wheel(s,b,v):0;}

void alt_core_trace_classify(const char*line,alt_trace_tags_t*tags) {
 if(tags) { memset(tags,0,sizeof(*tags)); tags->diag_class=-1; if(bound.classify) bound.classify(line,tags); }
}
