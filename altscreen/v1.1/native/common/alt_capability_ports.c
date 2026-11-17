#include "core/alt_capability_ports.h"
#include <pthread.h>
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static alt_capability_ports providers;
void alt_core_bind_capability_ports(const alt_capability_ports *p) {
    pthread_mutex_lock(&lock);
    if (p != 0) providers=*p;
    pthread_mutex_unlock(&lock);
}
unsigned int alt_core_available_capabilities(void) {
    alt_capability_ports p;
    pthread_mutex_lock(&lock);p=providers;pthread_mutex_unlock(&lock);
    return (p.input ? p.input() : 0u) | (p.output ? p.output() : 0u);
}

/* Bounded per-receiver capability negotiation; this is support/readiness
 * accounting, not a new CarPlay advertisement. The iPhone capabilities are
 * still negotiated by the stock AirPlay property path. */
#define CAP_SESSION_LIMIT 16
static struct {
    void *receiver;
    unsigned int supported;
    uint64_t epoch;
    int active;
} sessions[CAP_SESSION_LIMIT];
static uint64_t session_epoch;
static int find_session(void *receiver) {
    int i;
    for(i=0;i<CAP_SESSION_LIMIT;i++)
        if(sessions[i].receiver==receiver && sessions[i].active)return i;
    return -1;
}
void alt_core_capability_session_started(void *receiver) {
    if(!receiver)return;
    unsigned int supported=alt_core_available_capabilities();
    pthread_mutex_lock(&lock);
    int i=find_session(receiver);
    if(i<0) {
        for(i=0;i<CAP_SESSION_LIMIT;i++)if(!sessions[i].active)break;
        if(i==CAP_SESSION_LIMIT)i=0; /* diagnostics bounded; no heap growth */
    }
    sessions[i].receiver=receiver;
    sessions[i].supported=supported;
    sessions[i].active=1;
    sessions[i].epoch=++session_epoch;
    pthread_mutex_unlock(&lock);
}
void alt_core_capability_session_stopped(void *receiver) {
    pthread_mutex_lock(&lock);
    int i=find_session(receiver);
    if(i>=0) {sessions[i].active=0;sessions[i].epoch=++session_epoch;}
    pthread_mutex_unlock(&lock);
}
int alt_core_capability_session_active(void *receiver) {
    pthread_mutex_lock(&lock);
    int i=find_session(receiver);
    pthread_mutex_unlock(&lock);
    return i>=0;
}
unsigned int alt_core_session_supported_capabilities(void *receiver) {
    pthread_mutex_lock(&lock);
    int i=find_session(receiver);
    unsigned int caps=(i>=0)?sessions[i].supported:0u;
    pthread_mutex_unlock(&lock);
    return caps;
}
uint64_t alt_core_capability_session_epoch(void *receiver) {
    pthread_mutex_lock(&lock);
    int i=find_session(receiver);
    uint64_t epoch=(i>=0)?sessions[i].epoch:0;
    pthread_mutex_unlock(&lock);
    return epoch;
}

unsigned int alt_core_capabilities_for_receiver(void *receiver) {
    pthread_mutex_lock(&lock);
    int i=find_session(receiver);
    unsigned int snapshot=(i>=0)?sessions[i].supported:0u;
    pthread_mutex_unlock(&lock);
    return i>=0?snapshot:alt_core_available_capabilities();
}
