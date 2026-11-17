#include "core/alt_session_ports.h"
/* These are the exact original exported K2161 interposition entry points.
 * Inputs forward opaque platform handles through the universal port contract. */
__attribute__((visibility("hidden"))) extern void k2161_bind_session_ports(void);
int AirPlayReceiverSessionMakeModeStateFromDictionary(void *r,void *d,void *mode) {
  k2161_bind_session_ports();return alt_core_session_parse(r,d,mode);
}
int AirPlayReceiverSessionSetup(void *r,void *d,void **response) {
  k2161_bind_session_ports();return alt_core_session_setup(r,d,response);
}
void AirPlayReceiverSessionTearDown(void *r,void *d,int reason,unsigned char *done) {
  k2161_bind_session_ports();alt_core_session_teardown(r,d,reason,done);
}
int ScreenStreamProcessData(void *stream,const uint8_t *data,size_t len,uint64_t ticks,
     void *options,void (*completion)(void*),void *ctx) {
  k2161_bind_session_ports();return alt_core_session_process_data(stream,data,len,ticks,options,completion,ctx);
}
