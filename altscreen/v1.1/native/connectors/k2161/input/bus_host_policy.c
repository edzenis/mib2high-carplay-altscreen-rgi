/* K2161 mm-ipod / QNX 6.5 bus thread behavior. Original AltScreen semantics. */
#include "bus_host_policy.h"
#include <signal.h>
#include <pthread.h>
void k2161_bus_block_sigpipe(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    (void)pthread_sigmask(SIG_BLOCK, &set, NULL);
}
