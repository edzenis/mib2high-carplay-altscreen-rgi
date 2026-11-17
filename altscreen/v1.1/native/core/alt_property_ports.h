#ifndef ALT_CORE_PROPERTY_PORTS_H
#define ALT_CORE_PROPERTY_PORTS_H
#include <stdint.h>
/* Firmware-neutral opaque request/response port. All mutable OEM dictionaries
 * are constructed exclusively in platform output. */
typedef struct alt_core_property_ports {
 void *(*server_property)(void*,uint32_t,void*,void*,int*);
 void *(*session_property)(void*,uint32_t,void*,void*,int*);
 void *(*server_info)(void*,void*,uint8_t*,int*);
} alt_core_property_ports;
__attribute__((visibility("hidden"))) void alt_core_bind_properties(const alt_core_property_ports*);
__attribute__((visibility("hidden"))) void *alt_core_server_property(void*,uint32_t,void*,void*,int*);
__attribute__((visibility("hidden"))) void *alt_core_session_property(void*,uint32_t,void*,void*,int*);
__attribute__((visibility("hidden"))) void *alt_core_server_info(void*,void*,uint8_t*,int*);
#endif
