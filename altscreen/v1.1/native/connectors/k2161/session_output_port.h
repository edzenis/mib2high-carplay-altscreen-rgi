#ifndef K2161_SESSION_OUTPUT_PORT_H
#define K2161_SESSION_OUTPUT_PORT_H
#include <stddef.h>
#include <stdint.h>
/* Private K2161 composition-root declarations, never included by core/input. */
__attribute__((visibility("hidden"))) int k2161_output_parse_mode(void*,void*,void*);
__attribute__((visibility("hidden"))) int k2161_output_session_setup(void*,void*,void**);
__attribute__((visibility("hidden"))) void k2161_output_session_teardown(void*,void*,int,unsigned char*);
__attribute__((visibility("hidden"))) int k2161_output_screen_data(void*,const uint8_t*,size_t,uint64_t,void*,void (*)(void*),void*);
__attribute__((visibility("hidden"))) void *k2161_output_server_property(void*,uint32_t,void*,void*,int*);
__attribute__((visibility("hidden"))) void *k2161_output_session_property(void*,uint32_t,void*,void*,int*);
__attribute__((visibility("hidden"))) void *k2161_output_server_info(void*,void*,uint8_t*,int*);
#endif
