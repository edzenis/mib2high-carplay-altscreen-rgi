#include "alt_session_ports.h"
#include "alt_capability_ports.h"
#include <string.h>
static alt_session_ports bound;
/* Composition root binds once before any ingress invokes a port. */
void alt_core_bind_session_ports(const alt_session_ports *api) { if(api)memcpy(&bound,api,sizeof(bound)); }
int alt_core_session_parse(void *r,void *d,void *o) {return bound.parse_mode?bound.parse_mode(r,d,o):-1;}
int alt_core_session_setup(void *r,void *d,void **o) {
    int rc=bound.setup?bound.setup(r,d,o):-1;
    if(rc==0)alt_core_capability_session_started(r);
    return rc;
}
void alt_core_session_teardown(void *r,void *d,int reason,unsigned char *done) {
    if(bound.teardown)bound.teardown(r,d,reason,done);
    alt_core_capability_session_stopped(r);
}
int alt_core_session_process_data(void *stream,const uint8_t *data,size_t len,uint64_t ticks,
             void *options,void (*completion)(void *),void *ctx) {
 return bound.process_data?bound.process_data(stream,data,len,ticks,options,completion,ctx):-1;
}
