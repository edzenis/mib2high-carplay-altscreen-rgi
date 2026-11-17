#define _GNU_SOURCE
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#include "receive_probe.h"
#include "ident_hook.h"
#include "send_hook.h"
#include "message_table.h"
#include "rgi_bridge.h"
#include "diagnostics.h"
#include "status.h"

typedef void *(*real_dlopen_fn)(const char *, int);
typedef void *(*real_memcpy_fn)(void *, const void *, size_t);

static real_dlopen_fn real_dlopen_ptr;
static real_memcpy_fn real_memcpy_ptr;
static int resolving_memcpy;
static uintptr_t driver_base;
static int callsite_reported;
static unsigned int ident_install_attempts;
static int ident_install_complete;
static pthread_mutex_t hook_setup_lock = PTHREAD_MUTEX_INITIALIZER;
static int bridge_ok_reported;
static int bridge_error_reported;

volatile unsigned int k2161_rx_callsite_count;
volatile unsigned int k2161_rx_rgi_count;
volatile unsigned int k2161_rx_partial_rgi_count;
volatile unsigned int k2161_rx_5201;
volatile unsigned int k2161_rx_5202;
volatile unsigned int k2161_rx_5204;
volatile unsigned int k2161_rx_bridge_ok;
volatile unsigned int k2161_rx_bridge_fail;

static void *fallback_memcpy(void *dst, const void *src, size_t n)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    const volatile uint8_t *s = (const volatile uint8_t *)src;
    size_t i;
    for (i = 0; i < n; ++i)
        d[i] = s[i];
    return dst;
}

static void write_marker(const char *text, size_t len)
{
    /* Text-only bounded session diagnostics; no transport recording. */
    while (len && (text[len-1] == '\n' || text[len-1] == '\r')) --len;
    rgi_diag_log("RX", "%.*s", (int)len, text);
}

static real_memcpy_fn resolve_memcpy(void)
{
    if (real_memcpy_ptr != NULL)
        return real_memcpy_ptr;
    if (resolving_memcpy)
        return NULL;
    resolving_memcpy = 1;
    real_memcpy_ptr = (real_memcpy_fn)dlsym(RTLD_NEXT, "memcpy");
    resolving_memcpy = 0;
    return real_memcpy_ptr;
}

static void ensure_driver_hooks(const char *trigger);

static void resolve_driver_base(void *handle)
{
    void *ipod_module;
    void *blob_symbol;
    uintptr_t base_from_module;
    uintptr_t base_from_blob;
    void *scope = handle != NULL ? handle : RTLD_DEFAULT;

    if (driver_base != 0)
        return;

    ipod_module = dlsym(scope, "ipod_module");
    blob_symbol = dlsym(scope, "packet_addparam_blob");

    if (ipod_module == NULL || blob_symbol == NULL) {
        write_marker("K2161_RGI_RX driver_symbols_not_visible_yet\n",
                     sizeof("K2161_RGI_RX driver_symbols_not_visible_yet\n") - 1U);
        return;
    }

    base_from_module =
        (uintptr_t)ipod_module - (uintptr_t)0x2614c;

    base_from_blob =
        (uintptr_t)blob_symbol - (uintptr_t)0x1d0d8;

    if (base_from_module != base_from_blob) {
        write_marker("K2161_RGI_RX base_crosscheck_failed\n",
                     sizeof("K2161_RGI_RX base_crosscheck_failed\n") - 1U);
        return;
    }

    driver_base = base_from_module;
    k2161_status_base_ready();
    rgi_diag_log("IDENT", "base_validated driver_base=%p trigger=dlopen_or_ctor",
                 (void *)driver_base);

    write_marker("K2161_RGI_RX driver_base_ready\n",
                 sizeof("K2161_RGI_RX driver_base_ready\n") - 1U);
    /* dlopen returns only after stock linker relocations are complete. Install
     * the verified builder hook now, before the first IdentificationInformation;
     * the proven receive-callsite remains a fallback. */
    ensure_driver_hooks("driver_ready");
}

static void ensure_driver_hooks(const char *trigger)
{
    int ident_rc, msgtable_rc, send_rc;
    if (driver_base == 0 || ident_install_complete) return;
    pthread_mutex_lock(&hook_setup_lock);
    if (ident_install_complete || ident_install_attempts >= 3) {
        pthread_mutex_unlock(&hook_setup_lock);
        return;
    }
    ++ident_install_attempts;
    rgi_diag_log("IDENT", "ensure_attempt=%u trigger=%s",
                 ident_install_attempts, trigger);
    ident_rc = k2161_ident_try_install_base((void *)driver_base);
    rgi_diag_log("IDENT", "ensure_result=%d trigger=%s", ident_rc, trigger);
    if (ident_rc == 1) {
        ident_install_complete = 1;
        msgtable_rc = k2161_msgtable_try_install_base((void *)driver_base);
        send_rc = msgtable_rc == 1
                ? k2161_send_hook_try_install_base((void *)driver_base) : -1;
        rgi_diag_log("IDENT", "dependent_hooks msgtable=%d sender=%d",
                     msgtable_rc, send_rc);
    }
    pthread_mutex_unlock(&hook_setup_lock);
}

static void observe_transport_copy(const uint8_t *copied, size_t n)
{
    k2161_rgi_frame frame;
    k2161_rgi_frame *found = NULL;

    if (k2161_probe_rgi_start(copied, n, &frame))
        found = &frame;
    if (found == NULL)
        return;

    ++k2161_rx_rgi_count;
    if (!frame.complete)
        ++k2161_rx_partial_rgi_count;
    if (frame.msgid == 0x5201)
        ++k2161_rx_5201;
    else if (frame.msgid == 0x5202)
        ++k2161_rx_5202;
    else if (frame.msgid == 0x5204)
        ++k2161_rx_5204;
    {
        uint32_t diag_seq = rgi_diag_rx_begin(frame.msgid, frame.frame_len);
        k2161_send_note_rgi_rx(frame.msgid);

    /*
     * Only complete control frames enter Luka's parser.
     * Incomplete frames are counted/logged, never sent as valid navigation state.
     */
    if (frame.complete) {
        int bridge_rc = k2161_rgi_bridge_feed(frame.frame, frame.frame_len);

        if (bridge_rc == 1) {
            ++k2161_rx_bridge_ok;

            if (!bridge_ok_reported) {
                bridge_ok_reported = 1;
                write_marker("K2161_RGI_RX bridge_first_ok\n",
                             sizeof("K2161_RGI_RX bridge_first_ok\n") - 1U);
            }
        } else if (bridge_rc < 0) {
            ++k2161_rx_bridge_fail;

            if (!bridge_error_reported) {
                bridge_error_reported = 1;
                write_marker("K2161_RGI_RX bridge_feed_failed\n",
                             sizeof("K2161_RGI_RX bridge_feed_failed\n") - 1U);
            }
        }
    }

        rgi_diag_rx_end(diag_seq, frame.complete ? 1 : 0);
    }
    rgi_diag_log("RX", "msg=%04x len=%u complete=%d",
                 (unsigned)frame.msgid, (unsigned)frame.frame_len, frame.complete);
}

void *memcpy(void *dst, const void *src, size_t n)
{
    uintptr_t caller = (uintptr_t)__builtin_return_address(0);
    real_memcpy_fn fn = resolve_memcpy();
    void *result = fn != NULL ? fn(dst, src, n) : fallback_memcpy(dst, src, n);

    if (driver_base != 0 &&
        caller == driver_base + (uintptr_t)0x11428) {

        ++k2161_rx_callsite_count;

        ensure_driver_hooks("receive_callsite");

        if (!callsite_reported) {
            callsite_reported = 1;

            write_marker("K2161_RGI_RX callsite_ready\n",
                         sizeof("K2161_RGI_RX callsite_ready\n") - 1U);
        }

        observe_transport_copy((const uint8_t *)dst, n);
    }
    return result;
}

void *dlopen(const char *path, int mode)
{
    void *handle;

    if (real_dlopen_ptr == NULL)
        real_dlopen_ptr = (real_dlopen_fn)dlsym(RTLD_NEXT, "dlopen");
    if (real_dlopen_ptr == NULL)
        return NULL;
    handle = real_dlopen_ptr(path, mode);
    if (handle != NULL) {
        resolve_driver_base(handle);

        /*
         * Start the localhost route-guidance bus after the real K2161 driver
         * has loaded, before the first RGI receive callback where possible.
         */
        if (driver_base != 0)
            (void)k2161_rgi_bridge_init();
    }
    return handle;
}

__attribute__((constructor))
static void k2161_rx_observer_init(void)
{
    rgi_diag_reset_file();
    rgi_diag_log("INIT", "observer_constructor");
    (void)resolve_memcpy();
    real_dlopen_ptr = (real_dlopen_fn)dlsym(RTLD_NEXT, "dlopen");
    write_marker("K2161_RGI_RX observer_loaded\n",
                 sizeof("K2161_RGI_RX observer_loaded\n") - 1U);
    resolve_driver_base(NULL);
}

__attribute__((destructor))
static void k2161_rx_observer_fini(void)
{
    k2161_rgi_bridge_shutdown();

}
