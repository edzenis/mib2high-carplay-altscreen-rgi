#include "alt_property_ports.h"
#include <string.h>
static alt_core_property_ports ports;
void alt_core_bind_properties(const alt_core_property_ports *api){if(api)memcpy(&ports,api,sizeof(ports));}
void *alt_core_server_property(void*a,uint32_t b,void*c,void*d,int*e) {
 return ports.server_property?ports.server_property(a,b,c,d,e):0;
}
void *alt_core_session_property(void*a,uint32_t b,void*c,void*d,int*e) {
 return ports.session_property?ports.session_property(a,b,c,d,e):0;
}
void *alt_core_server_info(void*a,void*b,uint8_t*c,int*d) {
 return ports.server_info?ports.server_info(a,b,c,d):0;
}
