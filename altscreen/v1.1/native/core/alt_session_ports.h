#ifndef ALT_CORE_SESSION_PORTS_H
#define ALT_CORE_SESSION_PORTS_H
#include <stddef.h>
#include <stdint.h>
/* Raw pointers are opaque platform tokens: the core never reads OEM dictionaries.
 * The input owns interception, the output owns OEM response/composition.
 * No QNX/Audi/CarPlay library headers are included by this port. */
typedef struct alt_session_ports {
    int (*parse_mode)(void *receiver,void *params,void *mode);
    int (*setup)(void *receiver,void *request,void **response);
    void (*teardown)(void *receiver,void *request,int reason,unsigned char *done);
    int (*process_data)(void *stream,const uint8_t *data,size_t len,uint64_t ticks,
       void *options,void (*completion)(void *),void *completion_context);
} alt_session_ports;
__attribute__((visibility("hidden"))) void alt_core_bind_session_ports(const alt_session_ports *api);
__attribute__((visibility("hidden"))) int alt_core_session_parse(void *r,void *d,void *o);
__attribute__((visibility("hidden"))) int alt_core_session_setup(void *r,void *d,void **o);
__attribute__((visibility("hidden"))) void alt_core_session_teardown(void *r,void *d,int reason,unsigned char *done);
__attribute__((visibility("hidden"))) int alt_core_session_process_data(void *stream,const uint8_t *data,size_t len,uint64_t ticks,
                                  void *options,void (*completion)(void *),void *ctx);
#endif
