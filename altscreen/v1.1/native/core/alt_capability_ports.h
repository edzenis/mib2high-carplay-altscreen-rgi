#ifndef ALT_CORE_CAPABILITY_PORTS_H
#define ALT_CORE_CAPABILITY_PORTS_H
#include <stdint.h>
/* Capability gathering is bound by platform composition, never by outputs.
 * An output connector requests the negotiated core view without invoking
 * an input connector's private firmware functions. */
typedef unsigned int (*alt_capability_provider)(void);
typedef struct alt_capability_ports {
    alt_capability_provider input;
    alt_capability_provider output;
} alt_capability_ports;
__attribute__((visibility("hidden"))) void alt_core_bind_capability_ports(const alt_capability_ports *providers);
__attribute__((visibility("hidden"))) unsigned int alt_core_available_capabilities(void);
/* Session readiness is distinct from static firmware support. Capture the
 * receiver's supported bitmap when its stock setup succeeds; never infer
 * HUD BAP availability from a native AirPlay session. */
__attribute__((visibility("hidden"))) void alt_core_capability_session_started(void *receiver);
__attribute__((visibility("hidden"))) void alt_core_capability_session_stopped(void *receiver);
__attribute__((visibility("hidden"))) int alt_core_capability_session_active(void *receiver);
__attribute__((visibility("hidden"))) unsigned int alt_core_session_supported_capabilities(void *receiver);
__attribute__((visibility("hidden"))) uint64_t alt_core_capability_session_epoch(void *receiver);
/* Preserve legacy pre-setup dispatch while using the negotiated snapshot
 * for receivers that have completed stock setup. */
__attribute__((visibility("hidden"))) unsigned int alt_core_capabilities_for_receiver(void *receiver);
#endif
