#include <stdint.h>
#include "core/alt_property_ports.h"
__attribute__((visibility("hidden"))) extern void k2161_bind_session_ports(void);
void *AirPlayReceiverServerPlatformCopyProperty(void*a,uint32_t b,void*c,void*d,int*e) {
  k2161_bind_session_ports();return alt_core_server_property(a,b,c,d,e);
}
void *AirPlayReceiverSessionPlatformCopyProperty(void*a,uint32_t b,void*c,void*d,int*e) {
  k2161_bind_session_ports();return alt_core_session_property(a,b,c,d,e);
}
void *AirPlayCopyServerInfo(void*a,void*b,uint8_t*c,int*d) {
  k2161_bind_session_ports();return alt_core_server_info(a,b,c,d);
}
