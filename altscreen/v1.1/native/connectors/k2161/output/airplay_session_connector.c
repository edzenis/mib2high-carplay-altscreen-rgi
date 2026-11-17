#define _GNU_SOURCE

#include <arpa/inet.h>
#include <stdio.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "altscreen.h"
#include "core/alt_trace_ports.h"
#include "core/alt_core.h"
#include "core/alt_actions.h"
#include "core/alt_capability_ports.h"


#include "framework/logging.h"

#define ALTSCREEN_SESSION_MODULE "AltScreenSession"
#define ALTSCREEN_MAX_STATES 4
#define ALTSCREEN_TIMEOUT_SECS 10
#define ALTSCREEN_PRIMARY_SYNC_WAIT_MS 3000
#define ALTSCREEN_PRIMARY_SYNC_POLL_US 10000
#define ALTSCREEN_VIEW_COUNT 4
#define ALTSCREEN_VIEW_REACQUIRE_DELAY_MS 150ULL
#define ALTSCREEN_CONTROL_ACK_TIMEOUT_MS 2500ULL
#define ALTSCREEN_CONTROL_IDLE 0
#define ALTSCREEN_CONTROL_SHOW 1
#define ALTSCREEN_CONTROL_VIEW 2
#define ALTSCREEN_CONTROL_KEYFRAME 3
#define ALTSCREEN_ACTIVE_MARKER "/tmp/mib2high-lvds-active"
#define K_CFSTRING_UTF8 0x08000100U

typedef void *cf_ref;
typedef long cf_index;
typedef unsigned long cf_type_id;
typedef int os_status;

typedef struct {
    int32_t screen;
    int32_t main_audio;
    int32_t phone;
    int32_t speech;
    int32_t speech_detail;
    int32_t turns;
} airplay_mode_state;

typedef os_status (*session_setup_fn)(void *, cf_ref, cf_ref *);
typedef void (*session_teardown_fn)(void *, cf_ref, os_status, unsigned char *);
typedef os_status (*screen_create_fn)(void **);
typedef void (*screen_delete_fn)(void *);
typedef os_status (*screen_setup_fn)(void *, cf_ref, uint32_t);
typedef os_status (*screen_security_fn)(void *, const uint8_t[16], const uint8_t[16]);
typedef os_status (*screen_process_fn)(void *, void *, int);
typedef void (*screen_stop_fn)(void *);
typedef os_status (*screen_send_cmd_fn)(void *, char, const void *, size_t);
typedef os_status (*netsock_create_native_fn)(void **, int);
typedef os_status (*netsock_delete_fn)(void *);
typedef os_status (*screen_process_data_fn)(void *, const uint8_t *, size_t, uint64_t,
                                            cf_ref, void (*)(void *), void *);
typedef os_status (*send_command_fn)(void *, cf_ref, void (*)(os_status, cf_ref, void *), void *);
typedef os_status (*mode_state_parse_fn)(void *, cf_ref, airplay_mode_state *);

typedef cf_ref (*cf_string_create_fn)(cf_ref, const char *, uint32_t);
typedef int (*cf_string_get_cstring_fn)(cf_ref, char *, cf_index, uint32_t);
typedef cf_ref (*cf_dict_mutable_copy_fn)(cf_ref, cf_index, cf_ref);
typedef cf_ref (*cf_array_mutable_copy_fn)(cf_ref, cf_index, cf_ref);
typedef cf_ref (*cf_dict_create_mutable_fn)(cf_ref, cf_index, const void *, const void *);
typedef cf_ref (*cf_array_create_mutable_fn)(cf_ref, cf_index, const void *);
typedef const void *(*cf_dict_get_value_fn)(cf_ref, const void *);
typedef void (*cf_dict_set_value_fn)(cf_ref, const void *, const void *);
typedef int64_t (*cf_dict_get_i64_fn)(cf_ref, const void *, os_status *);
typedef os_status (*cf_dict_set_i64_fn)(cf_ref, const void *, int64_t);
typedef cf_index (*cf_dict_count_fn)(cf_ref);
typedef void (*cf_dict_get_keys_values_fn)(cf_ref, const void **, const void **);
typedef cf_index (*cf_array_count_fn)(cf_ref);
typedef const void *(*cf_array_value_fn)(cf_ref, cf_index);
typedef void (*cf_array_append_fn)(cf_ref, const void *);
typedef unsigned char (*cf_equal_fn)(cf_ref, cf_ref);
typedef cf_type_id (*cf_get_type_fn)(cf_ref);
typedef cf_type_id (*cf_type_getter_fn)(void);
typedef cf_ref (*cf_retain_fn)(cf_ref);
typedef void (*cf_release_fn)(cf_ref);
typedef cf_ref (*cf_number_create_fn)(cf_ref, int, const void *);

typedef const void *(*cf_container_retain_cb)(void *, const void *);
typedef void (*cf_container_release_cb)(void *, const void *);
typedef cf_ref (*cf_container_description_cb)(const void *);
typedef unsigned char (*cf_container_equal_cb)(const void *, const void *);

typedef struct {
    cf_index version;
    cf_container_retain_cb retain;
    cf_container_release_cb release;
    cf_container_description_cb copy_description;
    cf_container_equal_cb equal;
} cf_dictionary_value_callbacks;

typedef struct {
    session_setup_fn session_setup;
    session_teardown_fn session_teardown;
    screen_create_fn screen_create;
    screen_delete_fn screen_delete;
    screen_setup_fn screen_setup;
    screen_security_fn screen_security;
    screen_process_fn screen_process;
    screen_stop_fn screen_stop;
    screen_send_cmd_fn screen_send_cmd;
    netsock_create_native_fn netsock_create_native;
    netsock_delete_fn netsock_delete;
    screen_process_data_fn screen_process_data;
    send_command_fn send_command;

    /* Stock parser used behind our narrow TurnByTurn interposer. */
    mode_state_parse_fn mode_state_parse;

    cf_string_create_fn string_create;
    cf_string_get_cstring_fn string_get_cstring;
    cf_type_getter_fn string_type;
    cf_dict_mutable_copy_fn dict_mutable_copy;
    cf_array_mutable_copy_fn array_mutable_copy;
    cf_dict_create_mutable_fn dict_create_mutable;
    cf_array_create_mutable_fn array_create_mutable;
    cf_dict_get_value_fn dict_get_value;
    cf_dict_set_value_fn dict_set_value;
    cf_dict_get_i64_fn dict_get_i64;
    cf_dict_set_i64_fn dict_set_i64;
    cf_dict_count_fn dict_count;
    cf_dict_get_keys_values_fn dict_get_keys_values;
    cf_array_count_fn array_count;
    cf_array_value_fn array_value;
    cf_array_append_fn array_append;
    cf_equal_fn equal;
    cf_get_type_fn get_type;
    cf_type_getter_fn array_type;
    cf_type_getter_fn dict_type;
    cf_retain_fn retain;
    cf_release_fn release;
    cf_number_create_fn number_create;
    const void *dict_key_callbacks;
    const void *dict_value_callbacks;
    const void *array_callbacks;
    int resolved;
    int ready;
} altscreen_api;

typedef struct {
    void *receiver;
    void *screen;
    uint64_t connection_id;
    int listen_fd;
    int accepted_fd;
    uint16_t data_port;
    pthread_t worker;
    int worker_started;
    int screen_started;
    int show_sent;
    int stop_requested;
    int active;

    /* Firmware-agnostic decisions and command sequencing now live in core. */
    alt_core_state core;
    int control_inflight; /* adapter SendCommand reentrancy fence */
    uint64_t control_generation; /* firmware/session lifetime, never recycled */
} altscreen_state;

static altscreen_api g_api;
static altscreen_state g_states[ALTSCREEN_MAX_STATES];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_view_worker_started; /* 0=no, -1=starting, 1=running */
/* K2161: Joey's event IDs over the stock DSI tracer, no HMI/Java replacement. */
static int g_layout_slot; /* ViewArea slot: 0 small gauges, 1 big gauges. */
static int g_bap54_layout_known; /* Starts UNKNOWN; no inferred layout from factory defaults. */
static alt_core_zoom_state g_zoom; /* neutral queue/availability policy lives in core */
/* Diagnostic IDs do not alter zoom selection, queue ordering or command payload. */
static uint64_t g_altscreen_stats_event_serial;
static uint64_t g_altscreen_stats_keypanel_events;
static uint64_t g_altscreen_stats_keypanel_accepted;
static uint64_t g_altscreen_stats_keypanel_rejected;
static uint64_t g_altscreen_stats_keypanel_buffered;
static uint64_t g_control_generation_serial;
static uint64_t g_control_request_serial;

typedef struct {
    void *receiver;
    uint64_t generation;
    uint64_t token;
    int stage;
    int view;
} control_completion_context;


static const void *local_value_retain(void *allocator, const void *object)
{
    (void)allocator;
    return g_api.retain != NULL ? g_api.retain((cf_ref)object) : object;
}

static void local_value_release(void *allocator, const void *object)
{
    (void)allocator;
    if (g_api.release != NULL && object != NULL)
        g_api.release((cf_ref)object);
}

static unsigned char local_value_equal(const void *left, const void *right)
{
    if (g_api.equal == NULL)
        return left == right;
    return g_api.equal((cf_ref)left, (cf_ref)right);
}

static const cf_dictionary_value_callbacks g_local_value_callbacks = {
    0,
    local_value_retain,
    local_value_release,
    NULL,
    local_value_equal
};

static cf_ref dict_mutable_copy_compat(cf_ref allocator, cf_index capacity, cf_ref source)
{
    cf_ref out;
    cf_index count;
    cf_index i;
    const void **keys = NULL;
    const void **values = NULL;

    (void)allocator;
    (void)capacity;

    if (source == NULL || g_api.dict_create_mutable == NULL ||
        g_api.dict_count == NULL || g_api.dict_get_keys_values == NULL ||
        g_api.dict_set_value == NULL)
        return NULL;

    out = g_api.dict_create_mutable(NULL, 0,
                                    g_api.dict_key_callbacks,
                                    g_api.dict_value_callbacks);
    if (out == NULL)
        return NULL;

    count = g_api.dict_count(source);
    if (count <= 0)
        return out;

    keys = (const void **)malloc((size_t)count * sizeof(*keys));
    values = (const void **)malloc((size_t)count * sizeof(*values));
    if (keys == NULL || values == NULL) {
        free(keys);
        free(values);
        g_api.release(out);
        return NULL;
    }

    g_api.dict_get_keys_values(source, keys, values);
    for (i = 0; i < count; ++i)
        g_api.dict_set_value(out, keys[i], values[i]);

    free(keys);
    free(values);
    return out;
}

static void *sym(const char *name)
{
    return dlsym(RTLD_NEXT, name);
}

static void resolve_api(void)
{
    if (g_api.resolved)
        return;

    g_api.resolved = 1;

    g_api.session_setup = (session_setup_fn)sym("AirPlayReceiverSessionSetup");
    g_api.session_teardown = (session_teardown_fn)sym("AirPlayReceiverSessionTearDown");
    g_api.screen_create = (screen_create_fn)sym("AirPlayReceiverSessionScreen_Create");
    g_api.screen_delete = (screen_delete_fn)sym("AirPlayReceiverSessionScreen_Delete");
    g_api.screen_setup = (screen_setup_fn)sym("AirPlayReceiverSessionScreen_Setup");
    g_api.screen_security = (screen_security_fn)sym("AirPlayReceiverSessionScreen_SetSecurityInfo");
    g_api.screen_process = (screen_process_fn)sym("AirPlayReceiverSessionScreen_ProcessFrames");
    g_api.screen_stop = (screen_stop_fn)sym("AirPlayReceiverSessionScreen_StopSession");
    g_api.screen_send_cmd = (screen_send_cmd_fn)sym("AirPlayReceiverSessionScreen_SendCommand");
    g_api.netsock_create_native = (netsock_create_native_fn)sym("NetSocket_CreateWithNative");
    g_api.netsock_delete = (netsock_delete_fn)sym("NetSocket_Delete");
    g_api.screen_process_data = (screen_process_data_fn)sym("ScreenStreamProcessData");
    g_api.send_command = (send_command_fn)sym("AirPlayReceiverSessionSendCommand");

    g_api.mode_state_parse = (mode_state_parse_fn)sym("AirPlayReceiverSessionMakeModeStateFromDictionary");

    g_api.string_create = (cf_string_create_fn)sym("CFStringCreateWithCString");
    g_api.string_get_cstring=(cf_string_get_cstring_fn)sym("CFStringGetCString");
    g_api.string_type=(cf_type_getter_fn)sym("CFStringGetTypeID");
    g_api.array_mutable_copy = (cf_array_mutable_copy_fn)sym("CFArrayCreateMutableCopy");
    g_api.dict_create_mutable = (cf_dict_create_mutable_fn)sym("CFDictionaryCreateMutable");
    g_api.array_create_mutable = (cf_array_create_mutable_fn)sym("CFArrayCreateMutable");
    g_api.dict_get_value = (cf_dict_get_value_fn)sym("CFDictionaryGetValue");
    g_api.dict_set_value = (cf_dict_set_value_fn)sym("CFDictionarySetValue");
    g_api.dict_get_i64 = (cf_dict_get_i64_fn)sym("CFDictionaryGetInt64");
    g_api.dict_set_i64 = (cf_dict_set_i64_fn)sym("CFDictionarySetInt64");
    g_api.dict_count = (cf_dict_count_fn)sym("CFDictionaryGetCount");
    g_api.dict_get_keys_values = (cf_dict_get_keys_values_fn)sym("CFDictionaryGetKeysAndValues");
    g_api.array_count = (cf_array_count_fn)sym("CFArrayGetCount");
    g_api.array_value = (cf_array_value_fn)sym("CFArrayGetValueAtIndex");
    g_api.array_append = (cf_array_append_fn)sym("CFArrayAppendValue");
    g_api.equal = (cf_equal_fn)sym("CFEqual");
    g_api.get_type = (cf_get_type_fn)sym("CFGetTypeID");
    g_api.array_type = (cf_type_getter_fn)sym("CFArrayGetTypeID");
    g_api.dict_type = (cf_type_getter_fn)sym("CFDictionaryGetTypeID");
    g_api.retain = (cf_retain_fn)sym("CFRetain");
    g_api.release = (cf_release_fn)sym("CFRelease");
    g_api.number_create = (cf_number_create_fn)sym("CFNumberCreate");

    g_api.dict_key_callbacks = sym("kCFTypeDictionaryKeyCallBacks");
    if (g_api.dict_key_callbacks == NULL)
        g_api.dict_key_callbacks = sym("kCFLDictionaryKeyCallBacksCFLTypes");

    g_api.dict_value_callbacks = sym("kCFTypeDictionaryValueCallBacks");
    if (g_api.dict_value_callbacks == NULL)
        g_api.dict_value_callbacks = sym("kCFLDictionaryValueCallBacksCFLTypes");
    if (g_api.dict_value_callbacks == NULL)
        g_api.dict_value_callbacks = &g_local_value_callbacks;

    g_api.array_callbacks = sym("kCFTypeArrayCallBacks");
    if (g_api.array_callbacks == NULL)
        g_api.array_callbacks = sym("kCFLArrayCallBacksCFLTypes");

    g_api.dict_mutable_copy = (cf_dict_mutable_copy_fn)sym("CFDictionaryCreateMutableCopy");
    if (g_api.dict_mutable_copy == NULL)
        g_api.dict_mutable_copy = dict_mutable_copy_compat;

    g_api.ready =
        g_api.session_setup != NULL &&
        g_api.session_teardown != NULL &&
        g_api.screen_create != NULL &&
        g_api.screen_delete != NULL &&
        g_api.screen_setup != NULL &&
        g_api.screen_security != NULL &&
        g_api.screen_process != NULL &&
        g_api.screen_stop != NULL &&
        g_api.netsock_create_native != NULL &&
        g_api.netsock_delete != NULL &&
        g_api.screen_process_data != NULL &&
        g_api.send_command != NULL &&
        g_api.string_create != NULL &&
        g_api.dict_mutable_copy != NULL &&
        g_api.array_mutable_copy != NULL &&
        g_api.dict_create_mutable != NULL &&
        g_api.array_create_mutable != NULL &&
        g_api.dict_get_value != NULL &&
        g_api.dict_set_value != NULL &&
        g_api.dict_get_i64 != NULL &&
        g_api.dict_set_i64 != NULL &&
        g_api.dict_count != NULL &&
        g_api.dict_get_keys_values != NULL &&
        g_api.array_count != NULL &&
        g_api.array_value != NULL &&
        g_api.array_append != NULL &&
        g_api.equal != NULL &&
        g_api.get_type != NULL &&
        g_api.array_type != NULL &&
        g_api.dict_type != NULL &&
        g_api.retain != NULL &&
        g_api.release != NULL &&
        g_api.dict_key_callbacks != NULL &&
        g_api.dict_value_callbacks != NULL &&
        g_api.array_callbacks != NULL;

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "runtime API ready=%d optional_screen_quit=%d dict_copy=%s "
             "mode_parser=%d number_create=%d",
             g_api.ready, g_api.screen_send_cmd != NULL,
             sym("CFDictionaryCreateMutableCopy") != NULL ? "native" : "compat",
             g_api.mode_state_parse != NULL, g_api.number_create != NULL);

    if (!g_api.ready) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "runtime API missing setup=%d teardown=%d create=%d delete=%d screen_setup=%d security=%d process=%d stop=%d netsock_create=%d netsock_delete=%d process_data=%d send_command=%d cf_string=%d dict_copy=%d array_copy=%d dict_create=%d array_create=%d dict_get=%d dict_set=%d dict_get_i64=%d dict_set_i64=%d dict_count=%d dict_keys_values=%d array_count=%d array_value=%d array_append=%d equal=%d get_type=%d array_type=%d dict_type=%d retain=%d release=%d dict_keys=%d dict_values=%d array_callbacks=%d",
                  g_api.session_setup != NULL,
                  g_api.session_teardown != NULL,
                  g_api.screen_create != NULL,
                  g_api.screen_delete != NULL,
                  g_api.screen_setup != NULL,
                  g_api.screen_security != NULL,
                  g_api.screen_process != NULL,
                  g_api.screen_stop != NULL,
                  g_api.netsock_create_native != NULL,
                  g_api.netsock_delete != NULL,
                  g_api.screen_process_data != NULL,
                  g_api.send_command != NULL,
                  g_api.string_create != NULL,
                  g_api.dict_mutable_copy != NULL,
                  g_api.array_mutable_copy != NULL,
                  g_api.dict_create_mutable != NULL,
                  g_api.array_create_mutable != NULL,
                  g_api.dict_get_value != NULL,
                  g_api.dict_set_value != NULL,
                  g_api.dict_get_i64 != NULL,
                  g_api.dict_set_i64 != NULL,
                  g_api.dict_count != NULL,
                  g_api.dict_get_keys_values != NULL,
                  g_api.array_count != NULL,
                  g_api.array_value != NULL,
                  g_api.array_append != NULL,
                  g_api.equal != NULL,
                  g_api.get_type != NULL,
                  g_api.array_type != NULL,
                  g_api.dict_type != NULL,
                  g_api.retain != NULL,
                  g_api.release != NULL,
                  g_api.dict_key_callbacks != NULL,
                  g_api.dict_value_callbacks != NULL,
                  g_api.array_callbacks != NULL);
    }
}

static cf_ref cfs(const char *s)
{
    if (s == NULL || g_api.string_create == NULL)
        return NULL;
    return g_api.string_create(NULL, s, K_CFSTRING_UTF8);
}

static int is_array(cf_ref obj)
{
    return obj != NULL && g_api.get_type(obj) == g_api.array_type();
}

static int is_dict(cf_ref obj)
{
    return obj != NULL && g_api.get_type(obj) == g_api.dict_type();
}

static cf_ref dict_get(cf_ref dict, const char *name)
{
    cf_ref key;
    cf_ref value = NULL;

    if (dict == NULL || name == NULL)
        return NULL;
    key = cfs(name);
    if (key == NULL)
        return NULL;
    value = (cf_ref)g_api.dict_get_value(dict, key);
    g_api.release(key);
    return value;
}

static int64_t dict_get_i64(cf_ref dict, const char *name, int *ok)
{
    cf_ref key;
    os_status err = -1;
    int64_t value = 0;

    if (ok != NULL)
        *ok = 0;
    key = cfs(name);
    if (key == NULL)
        return 0;
    value = g_api.dict_get_i64(dict, key, &err);
    g_api.release(key);
    if (ok != NULL)
        *ok = (err == 0);
    return value;
}

static int dict_set_obj(cf_ref dict, const char *name, cf_ref value)
{
    cf_ref key;

    if (dict == NULL || value == NULL)
        return 0;
    key = cfs(name);
    if (key == NULL)
        return 0;
    g_api.dict_set_value(dict, key, value);
    g_api.release(key);
    return 1;
}

static int dict_set_i64(cf_ref dict, const char *name, int64_t value)
{
    cf_ref key;
    os_status rc;

    key = cfs(name);
    if (key == NULL)
        return 0;
    rc = g_api.dict_set_i64(dict, key, value);
    g_api.release(key);
    return rc == 0;
}

static int dict_set_string(cf_ref dict, const char *name, const char *value)
{
    cf_ref s;
    int ok;

    s = cfs(value);
    if (s == NULL)
        return 0;
    ok = dict_set_obj(dict, name, s);
    g_api.release(s);
    return ok;
}

static cf_ref new_dict(void)
{
    return g_api.dict_create_mutable(NULL, 0,
                                     g_api.dict_key_callbacks,
                                     g_api.dict_value_callbacks);
}

static cf_ref new_array(void)
{
    return g_api.array_create_mutable(NULL, 0, g_api.array_callbacks);
}

static uint64_t now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
}

static int array_contains_string(cf_ref array, const char *value)
{
    cf_ref needle;
    cf_index i, n;
    int found = 0;

    if (!is_array(array))
        return 0;
    needle = cfs(value);
    if (needle == NULL)
        return 0;
    n = g_api.array_count(array);
    for (i = 0; i < n; ++i) {
        cf_ref item = (cf_ref)g_api.array_value(array, i);
        if (item != NULL && g_api.equal(item, needle)) {
            found = 1;
            break;
        }
    }
    g_api.release(needle);
    return found;
}

static int append_string(cf_ref array, const char *value)
{
    cf_ref s = cfs(value);

    if (s == NULL)
        return 0;
    g_api.array_append(array, s);
    g_api.release(s);
    return 1;
}

static int set_reference_enabled_features(cf_ref response)
{
    cf_ref features = NULL;
    cf_ref alt = NULL;
    cf_ref views = NULL;

    if (!is_dict(response))
        return 0;

    features = new_array();
    alt = cfs("altScreen");
    views = cfs("viewAreas");
    if (features == NULL || alt == NULL || views == NULL)
        goto fail;

    g_api.array_append(features, alt);
    g_api.array_append(features, views);
    if (!dict_set_obj(response, "enabledFeatures", features))
        goto fail;

    g_api.release(views);
    g_api.release(alt);
    g_api.release(features);
    return 1;

fail:
    if (views != NULL) g_api.release(views);
    if (alt != NULL) g_api.release(alt);
    if (features != NULL) g_api.release(features);
    return 0;
}

static cf_ref clone_response_with_reference_features(cf_ref stock_response)
{
    cf_ref response;

    if (!is_dict(stock_response))
        return NULL;
    response = g_api.dict_mutable_copy(NULL, 0, stock_response);
    if (response == NULL)
        return NULL;
    if (!set_reference_enabled_features(response)) {
        g_api.release(response);
        return NULL;
    }
    return response;
}

/* Returns 1 for exact MAP UUID, 0 when no UUID was supplied,
 * -1 for an explicit foreign/invalid UUID. Never capture a foreign stream. */
static int altscreen_map_map_uuid_role(cf_ref desc)
{
    static const char *keys[]={"uuid","screenUUID","displayUUID"};
    unsigned int i;
    int found = 0;
    char buf[96];
    if (!is_dict(desc)) return -1;
    for (i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) {
        cf_ref value = dict_get(desc,keys[i]);
        if (value == NULL) continue;
        found = 1;
        if (!g_api.string_type || !g_api.string_get_cstring || !g_api.get_type ||
            g_api.get_type(value) != g_api.string_type() ||
            !g_api.string_get_cstring(value,buf,sizeof(buf),K_CFSTRING_UTF8))
            return -1;
        if (strcmp(buf,ALTSCREEN_UUID) != 0) return -1;
    }
    return found ? 1 : 0;
}

/* Reject mixed or explicitly foreign stream-111 requests before splitting.
 * No-UUID legacy MAP is accepted only as a single Type-111 descriptor.
 * Foreign streams are returned to the stock path completely unchanged. */
static int altscreen_map_foreign_stream111_present(cf_ref request)
{
    cf_ref streams;
    cf_index i;
    if (!is_dict(request)) return 0;
    streams = dict_get(request,"streams");
    if (!is_array(streams)) return 0;
    for (i = 0; i < g_api.array_count(streams); ++i) {
        cf_ref desc = (cf_ref)g_api.array_value(streams,i);
        int ok = 0;
        int64_t type = is_dict(desc) ? dict_get_i64(desc,"type",&ok) : 0;
        if (ok && type == AIRPLAY_STREAM_TYPE_ALT_SCREEN &&
            altscreen_map_map_uuid_role(desc) < 0) return 1;
    }
    return 0;
}

static void log_setup_request(void *receiver, cf_ref request)
{
    cf_ref streams;
    cf_index i, n;

    if (!is_dict(request)) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_REQUEST receiver=%p level=unknown request_not_dict=1",
                 receiver);
        return;
    }

    streams = dict_get(request, "streams");
    if (!is_array(streams)) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_REQUEST receiver=%p level=session streams_present=0",
                 receiver);
        return;
    }

    n = g_api.array_count(streams);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "SETUP_REQUEST receiver=%p level=stream streams_present=1 count=%ld",
             receiver, (long)n);

    for (i = 0; i < n; ++i) {
        cf_ref desc = (cf_ref)g_api.array_value(streams, i);
        int ok = 0;
        int64_t type = 0;

        if (is_dict(desc))
            type = dict_get_i64(desc, "type", &ok);

        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_STREAM receiver=%p index=%ld type_valid=%d type=%lld role=%d conn=%llu",
                 receiver, (long)i, ok, (long long)type, altscreen_map_map_uuid_role(desc),
                 (unsigned long long)dict_get_i64(desc,"streamConnectionID",NULL));
    }
}

static void classify_teardown_request(cf_ref request,
                                      int *has_alt,
                                      int *has_main,
                                      int *other_streams)
{
    cf_ref streams;
    cf_index i, n;

    if (has_alt != NULL) *has_alt = 0;
    if (has_main != NULL) *has_main = 0;
    if (other_streams != NULL) *other_streams = 0;

    if (!is_dict(request))
        return;

    streams = dict_get(request, "streams");
    if (!is_array(streams))
        return;

    n = g_api.array_count(streams);
    for (i = 0; i < n; ++i) {
        cf_ref desc = (cf_ref)g_api.array_value(streams, i);
        int ok = 0;
        int64_t type = 0;

        if (is_dict(desc))
            type = dict_get_i64(desc, "type", &ok);

        if (ok && type == AIRPLAY_STREAM_TYPE_ALT_SCREEN &&
            altscreen_map_map_uuid_role(desc) >= 0) {
            if (has_alt != NULL) *has_alt = 1;
        } else {
            if (other_streams != NULL) ++(*other_streams);
            if (ok && type == 110 && has_main != NULL) *has_main = 1;
        }
    }
}


static void log_setup_response_features(void *receiver,
                                        os_status rc,
                                        cf_ref response)
{
    cf_ref features;
    cf_index count;

    if (rc != 0) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_RESPONSE_FEATURES receiver=%p rc=%d skipped=setup_error",
                 receiver, rc);
        return;
    }

    if (!is_dict(response)) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_RESPONSE_FEATURES receiver=%p rc=%d response_dict=0",
                 receiver, rc);
        return;
    }

    features = dict_get(response, "enabledFeatures");
    if (!is_array(features)) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "SETUP_RESPONSE_FEATURES receiver=%p rc=%d present=0 "
                 "altScreen=0 viewAreas=0",
                 receiver, rc);
        return;
    }

    count = g_api.array_count(features);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "SETUP_RESPONSE_FEATURES receiver=%p rc=%d present=1 count=%ld "
             "altScreen=%d viewAreas=%d",
             receiver, rc, (long)count,
             array_contains_string(features, "altScreen"),
             array_contains_string(features, "viewAreas"));
}

static cf_ref build_stock_only_request(cf_ref request, cf_ref *out_alt_desc)
{
    cf_ref streams;
    cf_ref stock_request = NULL;
    cf_ref stock_streams = NULL;
    cf_index i, n;
    int alt_count = 0;

    if (out_alt_desc != NULL) *out_alt_desc = NULL;
    if (!is_dict(request))
        return NULL;

    streams = dict_get(request, "streams");
    if (!is_array(streams))
        return NULL;

    n = g_api.array_count(streams);
    stock_request = g_api.dict_mutable_copy(NULL, 0, request);
    stock_streams = new_array();
    if (stock_request == NULL || stock_streams == NULL)
        goto fail;

    for (i = 0; i < n; ++i) {
        cf_ref desc = (cf_ref)g_api.array_value(streams, i);
        int ok = 0;
        int64_t type;

        if (!is_dict(desc)) {
            g_api.array_append(stock_streams, desc);
            continue;
        }
        type = dict_get_i64(desc, "type", &ok);
        if (ok && type == AIRPLAY_STREAM_TYPE_ALT_SCREEN) {
            int role = altscreen_map_map_uuid_role(desc);
            if (role < 0) {
                LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                          "ALTSESSION_DIAG_FAIL component=MAP_SETUP reason=FOREIGN_UUID action=STOCK_PASSTHROUGH");
                goto fail;
            }
            ++alt_count;
            if (alt_count == 1 && out_alt_desc != NULL) *out_alt_desc = desc;
            continue;
        }
        g_api.array_append(stock_streams, desc);
    }

    if (alt_count == 0) {
        g_api.release(stock_streams);
        g_api.release(stock_request);
        return NULL;
    }
    if (alt_count > 1) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "request split refused alt_count=%d expected=1 primary_untouched=1",
                  alt_count);
        goto fail;
    }

    if (!dict_set_obj(stock_request, "streams", stock_streams))
        goto fail;

    g_api.release(stock_streams);
    return stock_request;

fail:
    if (stock_streams != NULL) g_api.release(stock_streams);
    if (stock_request != NULL) g_api.release(stock_request);
    if (out_alt_desc != NULL) *out_alt_desc = NULL;
    return NULL;
}

static cf_ref merge_altscreen_response(cf_ref stock_response,
                                       cf_ref requested_alt_desc, uint16_t port)
{
    cf_ref response = NULL;
    cf_ref streams = NULL;
    cf_ref out_streams = NULL;
    cf_ref alt_stream = NULL;

    if (stock_response != NULL && is_dict(stock_response))
        response = g_api.dict_mutable_copy(NULL, 0, stock_response);
    else
        response = new_dict();
    if (response == NULL ||
        !is_dict(requested_alt_desc))
        goto fail;
    if (!set_reference_enabled_features(response))
        goto fail;

    streams = dict_get(response, "streams");
    if (is_array(streams))
        out_streams = g_api.array_mutable_copy(NULL, g_api.array_count(streams) + 1, streams);
    else
        out_streams = new_array();

    /*
     * Preserve the exact peer-requested Stream-111 descriptor (including
     * streamConnectionID and any iOS-version-specific keys), then add only
     * the receiver-owned response fields.  This matches the vehicle-proven
     * Joey/Harman GEN2 handshake instead of synthesizing a minimal response.
     */
    if (out_streams == NULL) goto fail;
    if (is_dict(requested_alt_desc)) {
        alt_stream = g_api.dict_mutable_copy(NULL, 0, requested_alt_desc);
        if (alt_stream == NULL || !dict_set_i64(alt_stream,"dataPort",port) ||
            !dict_set_i64(alt_stream,"streamID",AIRPLAY_STREAM_TYPE_ALT_SCREEN)) goto fail;
        g_api.array_append(out_streams, alt_stream);
    }
    if (!dict_set_obj(response, "streams", out_streams))
        goto fail;

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_SETUP_RESPONSE map_port=%u descriptors_cloned=1 unknown_fields_preserved=1",
             (unsigned int)port);

    if (alt_stream) g_api.release(alt_stream);
    g_api.release(out_streams);
    return response;

fail:
    if (alt_stream != NULL) g_api.release(alt_stream);
    if (out_streams != NULL) g_api.release(out_streams);
    if (response != NULL) g_api.release(response);
    return NULL;
}

static int open_listener(uint16_t *out_port)
{
    int fd = -1;
    int one = 1;
    uint16_t listen_port = 6000U;

    if (out_port == NULL)
        return -1;

#ifdef AF_INET6
    {
        int off = 0;
        struct sockaddr_in6 addr6;
        socklen_t len6 = sizeof(addr6);

        fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        if (fd >= 0) {
            (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
#ifdef IPV6_V6ONLY
            (void)setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
#endif
            memset(&addr6, 0, sizeof(addr6));
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(listen_port);

            if (bind(fd, (struct sockaddr *)&addr6, sizeof(addr6)) == 0 &&
                listen(fd, 1) == 0 &&
                getsockname(fd, (struct sockaddr *)&addr6, &len6) == 0) {
                *out_port = ntohs(addr6.sin6_port);
                if (*out_port != 0) {
                    LOG_INFO(ALTSCREEN_SESSION_MODULE,
                             "listener ready port=%u family=IPv6-dualstack",
                             (unsigned int)*out_port);
                    return fd;
                }
            }
            close(fd);
            fd = -1;
        }
    }
#endif

    {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);

        fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd < 0)
            return -1;

        (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(listen_port);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
            LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                      "listener bind failed port=%u errno=%d primary_preserved=1",
                      (unsigned int)listen_port, errno);
            goto fail;
        }
        if (listen(fd, 1) != 0)
            goto fail;
        if (getsockname(fd, (struct sockaddr *)&addr, &len) != 0)
            goto fail;

        *out_port = ntohs(addr.sin_port);
        if (*out_port == 0)
            goto fail;
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "listener ready port=%u family=IPv4-fallback",
                 (unsigned int)*out_port);
        return fd;
    }

fail:
    if (fd >= 0)
        close(fd);
    return -1;
}

static int wake_listener(uint16_t port)
{
    int fd;
    int rc;

    if (port == 0)
        return 0;

#ifdef AF_INET6
    {
        struct sockaddr_in6 addr6;
        fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        if (fd >= 0) {
            memset(&addr6, 0, sizeof(addr6));
            addr6.sin6_family = AF_INET6;
            addr6.sin6_port = htons(port);
            addr6.sin6_addr.s6_addr[15] = 1; /* ::1 */
            rc = connect(fd, (struct sockaddr *)&addr6, sizeof(addr6));
            close(fd);
            if (rc == 0)
                return 1;
        }
    }
#endif

    {
        struct sockaddr_in addr;
        fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (fd < 0)
            return 0;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port);
        rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        close(fd);
        return rc == 0;
    }
}

static altscreen_state *find_state_receiver_locked(void *receiver)
{
    int i;

    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (g_states[i].active && g_states[i].receiver == receiver)
            return &g_states[i];
    }
    return NULL;
}


static altscreen_state *find_state_worker_locked(pthread_t thread)
{
    int i;

    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (g_states[i].active && g_states[i].worker_started &&
            pthread_equal(g_states[i].worker, thread))
            return &g_states[i];
    }
    return NULL;
}

static altscreen_state *alloc_state_locked(void *receiver)
{
    int i;

    if (find_state_receiver_locked(receiver) != NULL) return NULL;
    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (!g_states[i].active) {
            memset(&g_states[i], 0, sizeof(g_states[i]));
        alt_core_reset(&g_states[i].core);
            g_states[i].receiver = receiver;
            g_states[i].listen_fd = -1;
            g_states[i].accepted_fd = -1;
            g_states[i].core.current_view = -1;
            g_states[i].core.pending_view = -1;
            g_states[i].core.control_target_view = -1;
            g_states[i].core.last_ui_slot = -1;
            g_states[i].control_generation = ++g_control_generation_serial;
            g_states[i].active = 1;
            return &g_states[i];
        }
    }
    return NULL;
}

static int private111_active_for_receiver(void *receiver)
{
    altscreen_state *state;
    int active = 0;

    if (access(ALTSCREEN_ACTIVE_MARKER, F_OK) != 0)
        return 0;

    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state != NULL && state->active && state->screen_started &&
        !state->stop_requested)
        active = 1;
    pthread_mutex_unlock(&g_lock);
    return active;
}

static void note_route_state(void *receiver, int route_active, int *changed)
{
    altscreen_state *state;

    if (changed != NULL)
        *changed = 0;
    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state != NULL) {
        int route_changed = alt_core_update_route(&state->core, route_active);
        if (changed != NULL) *changed = route_changed;
    }
    pthread_mutex_unlock(&g_lock);
}

/* K2161 presentation mapping stays in the output connector, not the core.
 * Another firmware may have any number of views in any arrangement. */
static int k2161_view_for_route(int layout, int route_active)
{
    return layout * 2 + (route_active ? 1 : 0);
}
static int route_view_for_state(const altscreen_state *state)
{
    return k2161_view_for_route(g_layout_slot,
        state != NULL && state->core.route_known && state->core.route_active);
}

/* Route transitions must preserve the current gauge layout (0/1 or 2/3).
 * Choose the ViewArea from live route + layout state under the same lock used
 * by the DSITracer layout reader; do not hardcode the small-gauge views. */
static void queue_route_view_for_receiver(void *receiver, uint64_t delay_ms)
{
    altscreen_state *state;

    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state != NULL && state->active && !state->stop_requested) {
        alt_core_queue_view(&state->core, route_view_for_state(state), g_layout_slot, 0, now_ms() + delay_ms);
    }
    pthread_mutex_unlock(&g_lock);
}

static cf_ref make_event_request_for_uuid(const char *type, const char *url, const char *uuid)
{
    cf_ref request = NULL;
    cf_ref params = NULL;

    request = new_dict();
    params = new_dict();
    if (request == NULL || params == NULL)
        goto fail;

    if (!dict_set_string(request, "type", type) ||
        !dict_set_string(params, "uuid", uuid))
        goto fail;
    if (url != NULL && !dict_set_string(params, "url", url))
        goto fail;
    if (!dict_set_obj(request, "params", params))
        goto fail;

    g_api.release(params);
    return request;

fail:
    if (params != NULL) g_api.release(params);
    if (request != NULL) g_api.release(request);
    return NULL;
}

static cf_ref make_event_request(const char *type, const char *url)
{
    return make_event_request_for_uuid(type,url,ALTSCREEN_UUID);
}

static cf_ref make_view_request(int view)
{
    cf_ref request = NULL;
    cf_ref params = NULL;
    cf_ref adjacent = NULL;
    int i;

    if (view < 0 || view >= ALTSCREEN_VIEW_COUNT)
        return NULL;

    if (g_api.number_create == NULL) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "updateViewArea refused reason=CFNumberCreate_missing adjacency_required=1");
        return NULL;
    }

    request = make_event_request("updateViewArea", NULL);
    if (request == NULL)
        return NULL;
    params = dict_get(request, "params");
    if (!is_dict(params) || !dict_set_i64(params, "viewAreaIndex", view) ||
        !dict_set_i64(params, "animationDurationMillis", 0))
        goto fail;

    /* Joey GEN2 parity: adjacency belongs to the updateViewArea command. */
    adjacent = new_array();
    if (adjacent == NULL)
        goto fail;
    for (i = 0; i < ALTSCREEN_VIEW_COUNT; ++i) {
        int64_t v = i;
        cf_ref number;
        if (i == view)
            continue;
        number = g_api.number_create(NULL, 4, &v);
        if (number == NULL)
            goto fail;
        g_api.array_append(adjacent, number);
        g_api.release(number);
    }
    if (!dict_set_obj(params, "adjacentViewAreas", adjacent))
        goto fail;
    g_api.release(adjacent);
    adjacent = NULL;
    return request;

fail:
    if (adjacent != NULL) g_api.release(adjacent);
    if (request != NULL) g_api.release(request);
    return NULL;
}

typedef struct { uint64_t dispatch_id; uint64_t source_id; int direction; } altscreen_stats_zoom_ack_ctx;

/* Acknowledgment is observable only if the sender invokes its completion;
 * ACK=0 proves command transport acceptance, NOT visible map zoom. */
static void altscreen_stats_zoom_ack(os_status status, cf_ref response, void *opaque)
{
    altscreen_stats_zoom_ack_ctx *ctx = (altscreen_stats_zoom_ack_ctx *)opaque;
    if (!ctx) return;
    /* Log what is actually observable: callback response presence and type.
     * Do not infer a visually changed zoom from an accepted control request. */
    if (response && is_dict(response)) {
        int has_status = 0, has_error = 0;
        int64_t status_code = dict_get_i64(response, "status", &has_status);
        int64_t error_code = dict_get_i64(response, "errorCode", &has_error);
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "ALTZOOM_ZOOM_RESULT_FIELDS dispatch=%llu has_status=%d status_code=%lld has_errorCode=%d error_code=%lld",
                 (unsigned long long)ctx->dispatch_id, has_status,
                 (long long)status_code, has_error, (long long)error_code);
    }
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTZOOM_ZOOM_CALLBACK t_ms=%llu dispatch=%llu source=%llu response=%p response_type=%lu response_dict_keys=%ld status=%d",
             (unsigned long long)now_ms(), (unsigned long long)ctx->dispatch_id,
             (unsigned long long)ctx->source_id, response,
             response && g_api.get_type ? (unsigned long)g_api.get_type(response) : 0UL,
             response && g_api.get_type && g_api.dict_type && g_api.dict_count &&
             g_api.get_type(response) == g_api.dict_type()
                ? (long)g_api.dict_count(response) : -1L,
             status);
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_ZOOM_ACK t_ms=%llu dispatch=%llu source=%llu direction=%d status=%d visible_effect=UNKNOWN",
             (unsigned long long)now_ms(), (unsigned long long)ctx->dispatch_id,
             (unsigned long long)ctx->source_id, ctx->direction, status);
    free(ctx);
}

static os_status send_zoom_event(void *receiver, int direction, uint64_t dispatch_id, uint64_t source_id)
{
    cf_ref request = NULL, params = NULL;
    os_status rc;
    altscreen_stats_zoom_ack_ctx *ctx = NULL;
    alt_core_action intent = alt_core_action_zoom(direction);
    if (receiver == NULL || g_api.send_command == NULL ||
        !alt_core_can_dispatch(alt_core_capabilities_for_receiver(receiver), intent)) {
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "ALTSESSION_ZOOM_REFUSED t_ms=%llu dispatch=%llu receiver=%p direction=%d reason=invalid_api_or_direction",
                 (unsigned long long)now_ms(), (unsigned long long)dispatch_id, receiver, direction);
        return -1;
    }
    /* Joey's vehicle-tested control command_zoom(dir,1) includes the target
     * cluster display uuid. AltScreen omitted it; iOS accepted transport (rc=0)
     * but did not zoom the cluster. This is the focused AltScreen correction. */
    request = new_dict();
    params = new_dict();
    if (request == NULL || params == NULL ||
        !dict_set_string(request, "type", "changeMapZoomLevel") ||
        !dict_set_string(params, "uuid", ALTSCREEN_UUID) ||
        !dict_set_i64(params, "zoomDirection", direction) ||
        !dict_set_obj(request, "params", params)) {
        if (params != NULL) g_api.release(params);
        if (request != NULL) g_api.release(request);
        return -1;
    }
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTZOOM_ZOOM_REQUEST t_ms=%llu dispatch=%llu source=%llu request_type=changeMapZoomLevel target_uuid=%s zoomDirection=%d request_keys=%ld params_keys=%ld ready=%d",
             (unsigned long long)now_ms(), (unsigned long long)dispatch_id,
             (unsigned long long)source_id, ALTSCREEN_UUID, direction,
             g_api.dict_count ? (long)g_api.dict_count(request) : -1L,
             g_api.dict_count ? (long)g_api.dict_count(params) : -1L,
             g_api.ready);
    g_api.release(params);
    ctx = (altscreen_stats_zoom_ack_ctx *)calloc(1, sizeof(*ctx));
    if (ctx != NULL) {
        ctx->dispatch_id = dispatch_id;
        ctx->source_id = source_id;
        ctx->direction = direction;
    }
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_ZOOM_SEND t_ms=%llu dispatch=%llu source=%llu receiver=%p target=MAP uuid=%s direction=%d callback=%s",
             (unsigned long long)now_ms(), (unsigned long long)dispatch_id,
             (unsigned long long)source_id, receiver, ALTSCREEN_UUID, direction,
             ctx != NULL ? "registered" : "allocation_failed");
    rc = g_api.send_command(receiver, request, ctx != NULL ? altscreen_stats_zoom_ack : NULL, ctx);
    /* Like AltScreen, a successful synchronous submission transfers callback ownership.
     * No callback arrival means ACK remains unknown, and is flagged by diagnostics. */
    if (rc == 0) ctx = NULL;
    if (ctx != NULL) free(ctx);
    g_api.release(request);
    LOG_INFO(ALTSCREEN_SESSION_MODULE, "ALTSESSION_ZOOM direction=%d uuid=%s rc=%d ack=%s", direction, ALTSCREEN_UUID, rc,
             rc == 0 ? "PENDING_OR_INLINE" : "NOT_QUEUED");
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_ZOOM_SUBMIT t_ms=%llu dispatch=%llu source=%llu rc=%d ack=%s visual=UNKNOWN",
             (unsigned long long)now_ms(), (unsigned long long)dispatch_id,
             (unsigned long long)source_id, rc, rc == 0 ? "PENDING_OR_INLINE" : "NOT_QUEUED");
    return rc;
}

static os_status send_view_event(void *receiver, int view)
{
    cf_ref request;
    os_status rc;

    if (receiver == NULL || g_api.send_command == NULL)
        return -1;
    request = make_view_request(view);
    if (request == NULL)
        return -1;
    rc = g_api.send_command(receiver, request, NULL, NULL);
    g_api.release(request);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "event type=updateViewArea rc=%d view=%d adjacent=all_other_views",
             rc, view);
    return rc;
}

static os_status send_event_to_uuid(void *receiver, const char *type, const char *url, const char *uuid)
{
    cf_ref request;
    os_status rc = -1;

    if (receiver == NULL || g_api.send_command == NULL)
        return -1;
    request = make_event_request_for_uuid(type, url, uuid);
    if (request == NULL)
        return -1;

    rc = g_api.send_command(receiver, request, NULL, NULL);
    g_api.release(request);

    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_DIRECT_COMMAND t_ms=%llu type=%s receiver=%p target=%s uuid=%s url=%s rc=%d ack=UNOBSERVED",
             (unsigned long long)now_ms(), type, receiver,
             "MAP", uuid,
             url != NULL ? url : "-", rc);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "event type=%s rc=%d uuid=%s url=%s",
             type, rc, uuid, url != NULL ? url : "-");
    return rc;
}

static os_status send_event(void *receiver, const char *type, const char *url)
{
    return send_event_to_uuid(receiver,type,url,ALTSCREEN_UUID);
}

/* First real stream frame starts the UI sequence.  A fresh state/generation
 * forces showUI once, even if the Audi VC active marker is not yet present. */
static void request_cluster(altscreen_state *state)
{
    int view = -1;
    if (state == NULL) return;
    pthread_mutex_lock(&g_lock);
    if (state->active && !state->stop_requested) {
        view = route_view_for_state(state);
        alt_core_input event = {ALT_INPUT_START_VIDEO,view,view,g_layout_slot,now_ms(),0};
        (void)alt_core_input_event(&state->core,event);
    }
    pthread_mutex_unlock(&g_lock);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_CONTROL_STARTUP receiver=%p view=%d queued=%d",
             state->receiver, view, view >= 0);
}

int altscreen_request_fresh_keyframe(void)
{
    void *receiver = NULL;
    os_status rc;
    int i;

    pthread_mutex_lock(&g_lock);
    for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
        if (g_states[i].active && g_states[i].receiver != NULL &&
            g_states[i].screen_started && !g_states[i].stop_requested) {
            receiver = g_states[i].receiver;
            break;
        }
    }
    pthread_mutex_unlock(&g_lock);

    if (receiver == NULL) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "fresh keyframe request skipped reason=no_active_stream111");
        return 0;
    }

    rc = send_event(receiver, "forceKeyFrame", NULL);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "fresh keyframe request receiver=%p rc=%d source=renderer_reprime",
             receiver, rc);
    return rc == 0;
}

/* Format and values are verbatim from Joey r52/r55/r56. Never intercept stock wheel input. */
/* Passive trace samples; at most one line per class/second so diagnostics
 * cannot saturate the hook's bounded asynchronous logging queue.  These
 * observations NEVER change the sender or AirPlay/BAP command paths. */
static void altscreen_diagnostic_diag_trace_sample(const char *line, int buffered, const alt_trace_tags_t *tags)
{
    static uint64_t last_sample_ms[4];
    int class_id = -1, emit = 0;
    uint64_t now;
    /* DSI syntax was classified by the K2161 input adapter. */
    class_id = tags ? tags->diag_class : -1;
    if (class_id < 0) return;
    now = now_ms();
    pthread_mutex_lock(&g_lock);
    if (last_sample_ms[class_id] == 0 || now < last_sample_ms[class_id] ||
        now - last_sample_ms[class_id] >= 1000ULL) {
        last_sample_ms[class_id] = now;
        emit = 1;
    }
    pthread_mutex_unlock(&g_lock);
    if (emit)
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "DIAG_DSI_SAMPLE class=%d source=%s raw=%.360s",
                 class_id, buffered ? "buffered" : "live", line);
}

/* Authoritative K2161 VIEW-result state (not a steering-wheel press counter).
 * The AltScreen car trace verifies this indication appears on the LIVE port 1798
 * as a 2# subscriber record; 3#/4# are duplicate subscriptions.
 * Preserve the existing layout when the state is not yet known. */
static void altscreen_gauge_observe_gauge_bap54(const alt_core_trace_event *event)
{
    int slot, previous, first, i, queued = 0;
    unsigned int raw = 0;
    if (event->buffered || !event->gauge_valid)
        return; /* Buffered playback must NEVER override current live state. */
    slot = event->gauge_slot; raw = event->gauge_raw;
    pthread_mutex_lock(&g_lock);
    previous = g_layout_slot;
    first = !g_bap54_layout_known;
    g_bap54_layout_known = 1;
    if (slot != previous) {
        g_layout_slot = slot;
        for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
            altscreen_state *state = &g_states[i];
            if (!state->active || !state->screen_started || state->stop_requested)
                continue;
            alt_core_input event = {ALT_INPUT_LAYOUT,slot,route_view_for_state(state),slot,now_ms(),ALTSCREEN_VIEW_REACQUIRE_DELAY_MS};
            (void)alt_core_input_event(&state->core,event);
            ++queued;
        }
    }
    pthread_mutex_unlock(&g_lock);
    /* One-way K2161 output-side stage relay. This is the LIVE verified
     * gauge state, never buffered or guessed. The Java VC compositor output
     * reads it; it never feeds back into the input connector or shared core. */
    {
        FILE *gauge_out = fopen("/tmp/k2161-vc-gauge-slot", "w");
        if (gauge_out) {
            fputc(slot == 1 ? '1' : '0', gauge_out);
            fputc('\n', gauge_out);
            fclose(gauge_out);
        }
    }
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_GAUGE_BAP54 value=%u gauge=%s slot=%d previous_slot=%d "
             "first_state=%d queued=%d source=DSIBAP_live_1798",
             raw, slot == 1 ? "BIG" : "SMALL", slot, previous, first, queued);
}

static void altscreen_trace_trace_line(const alt_core_trace_event *event)
{
    const char *line = event->raw;
    int buffered = event->buffered;
    const alt_trace_tags_t traceTags = event->tags;
    int x, y, w, h, slot, step, i;
    uint64_t diag_keypanel_id = 0;
    /* BAP indications are 2#, not 1# on this car. Must process before
     * the original Joey 1# filter for zoom and viewport telemetry. */
    altscreen_gauge_observe_gauge_bap54(event);
    altscreen_diagnostic_diag_trace_sample(line, buffered, &traceTags);
    if (traceTags.has_keypanel) {
        uint64_t ev;
        int primary = traceTags.primary;
        pthread_mutex_lock(&g_lock);
        ev = ++g_altscreen_stats_event_serial;
        diag_keypanel_id = ev;
        ++g_altscreen_stats_keypanel_events;
        if (buffered) ++g_altscreen_stats_keypanel_buffered;
        pthread_mutex_unlock(&g_lock);
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "ALTSESSION_KEYPANEL_RAW t_ms=%llu event=%llu source=%s primary=%d raw=%.290s",
                 (unsigned long long)now_ms(), (unsigned long long)ev,
                 buffered ? "BUFFERED_20991" : "LIVE_1798", primary, line);
    }
    if (!traceTags.primary)
        return; /* Joey 1# and 5# are duplicates; 1# only for original signals */
    /* On K2161, the old A4 setZoomArea signal was absent in AltScreen car
     * traces. Capture alternative exact K2161 viewer-viewport attributes
     * passively; a viewport event is NOT yet a validated gauge-size change. */
    if (traceTags.has_viewport) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "ALTSESSION_GAUGE_CANDIDATE validated=0 source=%s raw=%.320s",
                 buffered ? "buffered" : "live", line);
    }
    if (traceTags.has_zoomarea) {
        if (!event->zoom_valid)
            return;
        slot=event->zoom_slot; x=event->zoom_x; y=event->zoom_y; w=event->zoom_w; h=event->zoom_h;
        pthread_mutex_lock(&g_lock);
        /* Once FctID 54 is observed, unverified ZoomArea size heuristics
         * must not reverse its confirmed absolute gauge state. */
        if (g_bap54_layout_known) {
            pthread_mutex_unlock(&g_lock);
            return;
        }
        if (slot != g_layout_slot) {
            g_layout_slot = slot;
            for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
                altscreen_state *state = &g_states[i];
                if (!state->active || !state->screen_started || state->stop_requested)
                    continue;
                alt_core_input event = {ALT_INPUT_LAYOUT,slot,route_view_for_state(state),slot,now_ms(),ALTSCREEN_VIEW_REACQUIRE_DELAY_MS};
                (void)alt_core_input_event(&state->core,event);
            }
            LOG_INFO(ALTSCREEN_SESSION_MODULE,
                     "ALTSESSION_LAYOUT slot=%d Rect=%d:%d:%d:%d view_index=%d source=%s",
                     slot, x,y,w,h, slot*2, buffered ? "buffered" : "live");
        }
        pthread_mutex_unlock(&g_lock);
        return;
    }
    /* Passive classification: record all primary encoder movements, including
     * 4/44 and 1/5, but NEVER reinterpret them as zoom without evidence. */
    {

        uint64_t ev = diag_keypanel_id;
        int before = 0, after = 0, accepted = 0;
        const char *reason;
        if (!traceTags.has_encoder) return;
        if (buffered) {
            LOG_WARN(ALTSCREEN_SESSION_MODULE,
                     "ALTSESSION_WHEEL_DECISION t_ms=%llu source=BUFFERED_20991 verdict=DROP reason=buffered_replay",
                     (unsigned long long)now_ms());
            return;
        }
        if (!traceTags.matches_zoom_encoder) {
            pthread_mutex_lock(&g_lock);
            ++g_altscreen_stats_keypanel_rejected;
            pthread_mutex_unlock(&g_lock);
            LOG_WARN(ALTSCREEN_SESSION_MODULE,
                     "ALTSESSION_WHEEL_DECISION t_ms=%llu source_event=%llu source=LIVE_1798 verdict=DROP reason=encoder_not_4_40 raw=%.240s",
                     (unsigned long long)now_ms(), (unsigned long long)ev, line);
            return;
        }
        if (!event->wheel_valid) return;
        step = event->wheel_step;
        pthread_mutex_lock(&g_lock);
        before = g_zoom.pending;
        if (alt_core_zoom_enqueue(&g_zoom, step, ev)) {
            ++g_altscreen_stats_keypanel_accepted;
            accepted = 1;
        } else ++g_altscreen_stats_keypanel_rejected;
        after = g_zoom.pending;
        pthread_mutex_unlock(&g_lock);
        reason = step == 0 ? "zero_step" : (accepted ? "matched_4_40" : "queue_saturated");
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "ALTZOOM_ZOOM_INPUT t_ms=%llu source_event=%llu step=%d accepted=%d raw=%.220s",
                 (unsigned long long)now_ms(), (unsigned long long)ev, step, accepted, line);
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "ALTSESSION_WHEEL_DECISION t_ms=%llu queue_source=%llu verdict=%s reason=%s step=%d before=%d after=%d uuid=%s",
                 (unsigned long long)now_ms(), (unsigned long long)ev,
                 accepted ? "ENQUEUE" : "DROP", reason, step, before, after, ALTSCREEN_UUID);
    }
}

static void k2161_trace_metrics(uint64_t *total, uint64_t *accepted,
                                uint64_t *rejected, uint64_t *buffered)
{
    pthread_mutex_lock(&g_lock);
    *total = g_altscreen_stats_keypanel_events;
    *accepted = g_altscreen_stats_keypanel_accepted;
    *rejected = g_altscreen_stats_keypanel_rejected;
    *buffered = g_altscreen_stats_keypanel_buffered;
    pthread_mutex_unlock(&g_lock);
}
static void altscreen_trace_start_tracers(void) {
    alt_core_start_trace(altscreen_trace_trace_line, k2161_trace_metrics);
}

/* Commands advance ONLY after their actual AirPlay SendCommand completion.
 * rc==0 from SendCommand means queued, not acknowledged.  A late callback is
 * generation- and token-checked and cannot touch a reused receiver/state. */
static void altscreen_control_control_completion(os_status status, cf_ref response, void *opaque)
{
    control_completion_context *ctx = (control_completion_context *)opaque;
    altscreen_state *state;
    int accepted = 0, next_stage = 0, last_slot = -1;
    (void)response;
    if (ctx == NULL) return;
    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(ctx->receiver);
    if (state != NULL && !state->stop_requested &&
        state->control_generation == ctx->generation &&
        state->core.control_awaiting_ack &&
        state->core.control_token == ctx->token &&
        state->core.control_stage == ctx->stage) {
        accepted = 1;
        (void)alt_core_ack(&state->core, ctx->token, ctx->stage, ctx->view, status);
        next_stage = state->core.control_stage;
        last_slot = state->core.last_ui_slot;
    }
    pthread_mutex_unlock(&g_lock);
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_CONTROL_ACK t_ms=%llu receiver=%p token=%llu stage=%d view=%d status=%d valid=%d next=%d target=MAP visible_effect=UNKNOWN",
             (unsigned long long)now_ms(), ctx->receiver, (unsigned long long)ctx->token,
             ctx->stage, ctx->view, status, accepted, next_stage);
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_CONTROL_ACK receiver=%p token=%llu stage=%d view=%d "
             "status=%d valid=%d next=%d last_slot=%d",
             ctx->receiver, (unsigned long long)ctx->token, ctx->stage,
             ctx->view, status, accepted, next_stage, last_slot);
    free(ctx);
}

/* Used after synchronous transport refusal (no completion is expected). */
static void altscreen_control_control_dispatch_failed(void *receiver, uint64_t generation,
                                        uint64_t token, int stage, int view, os_status rc)
{
    altscreen_state *state;
    int accepted = 0;
    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state != NULL && state->control_generation == generation &&
        state->core.control_awaiting_ack && state->core.control_token == token &&
        state->core.control_stage == stage) {
        (void)alt_core_dispatch_failed(&state->core, token, stage);
        accepted = 1;
    }
    pthread_mutex_unlock(&g_lock);
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_CONTROL_DISPATCH_FAIL receiver=%p token=%llu stage=%d "
             "view=%d rc=%d valid=%d", receiver,
             (unsigned long long)token, stage, view, rc, accepted);
}

static void altscreen_control_control_send(void *receiver, uint64_t generation, uint64_t token,
                             int stage, int view)
{
    control_completion_context *ctx;
    cf_ref request = NULL;
    os_status rc = -1;
    const char *url = NULL;
    alt_core_action intent = alt_core_action_for_stage(stage, view);
    if (!alt_core_can_dispatch(alt_core_capabilities_for_receiver(receiver), intent))
        intent.kind = ALT_ACTION_NONE;
    if (intent.kind == ALT_ACTION_SHOW_UI) {
        url = ALTSCREEN_MAP_RICH_URL;
        request = make_event_request("showUI", url);
    } else if (intent.kind == ALT_ACTION_UPDATE_VIEW) {
        request = make_view_request(intent.view);
    } else if (intent.kind == ALT_ACTION_FORCE_KEYFRAME) {
        request = make_event_request("forceKeyFrame", NULL);
    }
    ctx = (control_completion_context *)calloc(1, sizeof(*ctx));
    if (request != NULL && ctx != NULL && g_api.send_command != NULL) {
        ctx->receiver = receiver;
        ctx->generation = generation;
        ctx->token = token;
        ctx->stage = stage;
        ctx->view = view;
        rc = g_api.send_command(receiver, request, altscreen_control_control_completion, ctx);
        /* On rc==0, the completion owns ctx, even when called synchronously. */
        if (rc == 0) ctx = NULL;
    }
    if (request != NULL) g_api.release(request);
    if (ctx != NULL) free(ctx);
    pthread_mutex_lock(&g_lock);
    {
        altscreen_state *state = find_state_receiver_locked(receiver);
        if (state != NULL && state->control_generation == generation &&
            state->control_inflight > 0) --state->control_inflight;
    }
    pthread_mutex_unlock(&g_lock);
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_CONTROL_SEND t_ms=%llu receiver=%p token=%llu generation=%llu stage=%d view=%d target=MAP uuid=%s url=%s rc=%d ack=%s",
             (unsigned long long)now_ms(), receiver, (unsigned long long)token,
             (unsigned long long)generation, stage, view, ALTSCREEN_UUID,
             url != NULL ? url : "-", rc, rc == 0 ? "PENDING_OR_INLINE" : "NOT_QUEUED");
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_CONTROL_DISPATCH receiver=%p token=%llu stage=%d view=%d "
             "rc=%d url=%s ack=%s", receiver,
             (unsigned long long)token, stage, view, rc,
             url != NULL ? url : "-", rc == 0 ? "PENDING_OR_INLINE" : "NOT_QUEUED");
    if (rc != 0)
        altscreen_control_control_dispatch_failed(receiver, generation, token, stage, view, rc);
}

static void altscreen_control_control_tick(void)
{
        void *receiver = NULL;
        uint64_t generation = 0, token = 0;
        altscreen_state *picked = NULL;
        int view = -1, stage = ALTSCREEN_CONTROL_IDLE;
        int zoom = 0, i;
        uint64_t zoom_source = 0, zoom_dispatch = 0;
        alt_core_zoom_plan zoom_plan;
        uint64_t now = now_ms();
        int active = access(ALTSCREEN_ACTIVE_MARKER, F_OK) == 0;

        pthread_mutex_lock(&g_lock);
        for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
            altscreen_state *state = &g_states[i];
            if (!alt_core_session_ready(state->active && state->receiver != NULL,
                                            state->screen_started,state->stop_requested)) continue;
            {
                alt_core_plan plan;
                int ready;
                /* The core owns pending-state arbitration, timeouts, stages,
                 * tokens, and ACK deadlines. Only the receiver/lock/VC-marker
                 * checks and output dispatch belong to this connector. */
                int had_timeout = state->core.control_awaiting_ack && now >= state->core.control_deadline_ms;
                uint64_t expired_token = state->core.control_token;
                int expired_stage = state->core.control_stage;
                int expired_view = state->core.control_target_view;
                ready=alt_core_make_plan(&state->core,now,active,g_control_request_serial+1,&plan);
                if (had_timeout)
                    LOG_WARN(ALTSCREEN_SESSION_MODULE,
                         "ALTSESSION_CONTROL_TIMEOUT receiver=%p token=%llu stage=%d "
                         "view=%d next_state_change_required=1",
                         state->receiver,(unsigned long long)expired_token,expired_stage,expired_view);
                if (plan.promoted && plan.old_target!=plan.desired_view)
                    LOG_INFO(ALTSCREEN_SESSION_MODULE,
                         "ALTSESSION_CONTROL_LATEST receiver=%p old=%d latest=%d "
                         "origin=%s next=%d",state->receiver,plan.old_target,
                         plan.desired_view,
                         state->core.control_target_from_gauge?"gauge":"route",
                         state->core.control_stage);
                if (!ready) continue;
                receiver=state->receiver;
                generation=state->control_generation;
                view=plan.view;stage=plan.stage;token=plan.token;
                ++g_control_request_serial;
                state->control_inflight++;
            }
            break;
        }
        /* Connector selects an eligible native receiver. Neutral queue policy
         * decides SEND / DROP / WAIT and rate-limited diagnostics. */
        if (g_zoom.pending != 0) {
            for (i = 0; i < ALTSCREEN_MAX_STATES; ++i) {
                altscreen_state *state = &g_states[i];
                if (alt_core_session_ready(state->active && state->receiver != NULL,
                                           state->screen_started,state->stop_requested)) {
                    picked = state; break;
                }
            }
        }
        alt_core_zoom_plan_next(&g_zoom,picked != NULL,active,now,&zoom_plan);
        if (zoom_plan.result == ALT_ZOOM_SEND) {
            zoom=zoom_plan.step;
            zoom_source=zoom_plan.source_id;
            zoom_dispatch=zoom_plan.dispatch_id;
            LOG_WARN(ALTSCREEN_SESSION_MODULE,
                     "ALTZOOM_ZOOM_ROUTE t_ms=%llu dispatch=%llu source=%llu raw_step=%d command_direction=%d map_state_active=%d screen_started=%d stopping=%d marker=%d generation=%llu",
                     (unsigned long long)now, (unsigned long long)zoom_dispatch,
                     (unsigned long long)zoom_source, zoom, zoom > 0 ? 0 : 1,
                     picked->active, picked->screen_started, picked->stop_requested,
                     active, (unsigned long long)picked->control_generation);
            picked->control_inflight++;
            LOG_WARN(ALTSCREEN_SESSION_MODULE,
                     "ALTSESSION_ZOOM_DEQUEUE t_ms=%llu dispatch=%llu source=%llu marker=1 picked_role=%s receiver=%p remaining=%d target=MAP",
                     (unsigned long long)now, (unsigned long long)zoom_dispatch,
                     (unsigned long long)zoom_source, "MAP",
                     picked->receiver, zoom_plan.remaining);
        } else if (zoom_plan.result == ALT_ZOOM_DROP) {
            LOG_WARN(ALTSCREEN_SESSION_MODULE,
                     "ALTSESSION_ZOOM_DROP t_ms=%llu reason=vc_marker_inactive pending=%d",
                     (unsigned long long)now, zoom_plan.dropped);
        } else if (zoom_plan.result == ALT_ZOOM_BLOCKED && zoom_plan.report_blocked) {
            LOG_WARN(ALTSCREEN_SESSION_MODULE,
                     "ALTSESSION_ZOOM_BLOCKED t_ms=%llu reason=no_active_map_session pending=%d marker=%d",
                     (unsigned long long)now, zoom_plan.remaining, active);
        }
        pthread_mutex_unlock(&g_lock);

        if (zoom != 0 && picked != NULL) {
            (void)send_zoom_event(picked->receiver, zoom > 0 ? 0 : 1, zoom_dispatch, zoom_source);
            pthread_mutex_lock(&g_lock);
            if (picked->control_inflight > 0) picked->control_inflight--;
            pthread_mutex_unlock(&g_lock);
        }
        if (receiver != NULL && stage != ALTSCREEN_CONTROL_IDLE)
            altscreen_control_control_send(receiver, generation, token, stage, view);
}

static void *view_control_worker(void *arg)
{
    (void)arg;
    for (;;) {
        altscreen_control_control_tick();
        usleep(50000);
    }
    return NULL;
}

static void ensure_view_worker_started(void)
{
    pthread_t worker;
    int do_start = 0;

    pthread_mutex_lock(&g_lock);
    if (g_view_worker_started == 0) {
        g_view_worker_started = -1;
        do_start = 1;
    }
    pthread_mutex_unlock(&g_lock);

    if (!do_start)
        return;

    if (pthread_create(&worker, NULL, view_control_worker, NULL) == 0) {
        pthread_detach(worker);
        altscreen_trace_start_tracers();
        pthread_mutex_lock(&g_lock);
        g_view_worker_started = 1;
        pthread_mutex_unlock(&g_lock);
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "VIEW_WORKER started reason=private111_prepared");
    } else {
        pthread_mutex_lock(&g_lock);
        g_view_worker_started = 0;
        pthread_mutex_unlock(&g_lock);
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "VIEW_WORKER start_failed failsoft=1");
    }
}

/*
 * Narrow native compass-suppression seam.  Exact K2161 SessionControl calls
 * AirPlayReceiverSessionMakeModeStateFromDictionary through the PLT.  By
 * interposing the parser rather than patching SessionControl text or the Java
 * boot classpath, we can preserve the phone's real state privately and change
 * only the value returned to stock libairplay.
 *
 * kAirPlayEntity_Controller == 1 (phone owns turn-by-turn)
 * kAirPlayEntity_NotApplicable == 0
 */
os_status k2161_output_parse_mode(void *receiver,
                                                              cf_ref params,
                                                              airplay_mode_state *out)
{
    mode_state_parse_fn real;
    os_status rc;
    int original_turns;
    int route;
    int changed = 0;

    resolve_api();
    real = g_api.mode_state_parse;
    if (real == NULL)
        return -1;

    rc = real(receiver, params, out);
    if (rc != 0 || out == NULL || !private111_active_for_receiver(receiver))
        return rc;

    original_turns = out->turns;
    route = (original_turns == 1);
    note_route_state(receiver, route, &changed);

    /* Observe phone route state for our ViewArea logic; leave stock ownership unchanged. */
    /* AltScreen intentionally does not modify out->turns. */

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "TURN_STATE original=%d stock=%d route=%d changed=%d policy=OBSERVE_ONLY",
             original_turns, out->turns, route, changed);

    /* Never send AirPlay commands recursively from the parser callback. */
    if (changed) {
        int view = -1;
        pthread_mutex_lock(&g_lock);
        {
            altscreen_state *state = find_state_receiver_locked(receiver);
            if (state != NULL) view = route_view_for_state(state);
        }
        pthread_mutex_unlock(&g_lock);
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "DIAG_RGI_DECISION_OBSERVE phone_route=%d private_stream111=1 "
                 "active_marker=%d requested_view=%d java_hud_untouched=1 "
                 "cluster_tile_actual=UNKNOWN",
                 route, access(ALTSCREEN_ACTIVE_MARKER, F_OK) == 0, view);
        queue_route_view_for_receiver(receiver,
                                      ALTSCREEN_VIEW_REACQUIRE_DELAY_MS);
    }
    return rc;
}

static void *altscreen_worker(void *arg)
{
    altscreen_state *state = (altscreen_state *)arg;
    struct sockaddr_storage peer;
    socklen_t peer_len = sizeof(peer);
    int fd;
    int stop_requested;
    void *net = NULL;
    void *delegate_context;
    os_status rc;

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "worker waiting receiver=%p port=%u",
             state->receiver, (unsigned int)state->data_port);

    do {
        fd = accept(state->listen_fd, (struct sockaddr *)&peer, &peer_len);
    } while (fd < 0 && errno == EINTR);

    pthread_mutex_lock(&g_lock);
    if (state->listen_fd >= 0) {
        close(state->listen_fd);
        state->listen_fd = -1;
    }
    state->accepted_fd = fd;
    stop_requested = state->stop_requested;
    pthread_mutex_unlock(&g_lock);

    if (fd < 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "accept failed receiver=%p errno=%d stop=%d",
                  state->receiver, errno, stop_requested);
        return NULL;
    }

    if (stop_requested) {
        close(fd);
        pthread_mutex_lock(&g_lock);
        state->accepted_fd = -1;
        pthread_mutex_unlock(&g_lock);
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "worker wake consumed receiver=%p teardown=1",
                 state->receiver);
        return NULL;
    }

    /*
     * K2161/iOS can SETUP and connect Stream 111 before the stock Stream 110
     * screen has installed its time synchronizer.  Do not fail the private
     * Screen B in that ordering.  The worker is independent from the stock
     * AirPlay setup thread, so it is safe to hold the accepted socket briefly
     * while waiting for the primary screen to publish its synchronizer.
     */
    if (!altscreen_time_sync_ready()) {
        unsigned int waited_ms = 0;
        int stop_wait = 0;

        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "primary sync wait receiver=%p screen=%p timeout_ms=%u",
                 state->receiver, state->screen,
                 (unsigned int)ALTSCREEN_PRIMARY_SYNC_WAIT_MS);

        while (!altscreen_time_sync_ready() &&
               waited_ms < ALTSCREEN_PRIMARY_SYNC_WAIT_MS) {
            pthread_mutex_lock(&g_lock);
            stop_wait = state->stop_requested;
            pthread_mutex_unlock(&g_lock);
            if (stop_wait)
                break;
            usleep(ALTSCREEN_PRIMARY_SYNC_POLL_US);
            waited_ms += ALTSCREEN_PRIMARY_SYNC_POLL_US / 1000U;
        }

        pthread_mutex_lock(&g_lock);
        stop_wait = state->stop_requested;
        pthread_mutex_unlock(&g_lock);

        if (stop_wait || !altscreen_time_sync_ready()) {
            LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                      "primary sync wait failed receiver=%p waited_ms=%u stop=%d",
                      state->receiver, waited_ms, stop_wait);
            close(fd);
            pthread_mutex_lock(&g_lock);
            state->accepted_fd = -1;
            pthread_mutex_unlock(&g_lock);
            return NULL;
        }

        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "primary sync ready receiver=%p waited_ms=%u",
                 state->receiver, waited_ms);
    }

    delegate_context = altscreen_main_delegate_context();
    rc = altscreen_start_session(state->screen, delegate_context);
    if (rc != 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "session start failed receiver=%p rc=%d",
                  state->receiver, rc);
        close(fd);
        pthread_mutex_lock(&g_lock);
        state->accepted_fd = -1;
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }

    pthread_mutex_lock(&g_lock);
    state->screen_started = 1;
    pthread_mutex_unlock(&g_lock);


    rc = g_api.netsock_create_native(&net, fd);
    if (rc != 0 || net == NULL) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "NetSocket create failed receiver=%p rc=%d",
                  state->receiver, rc);
        close(fd);
        pthread_mutex_lock(&g_lock);
        state->accepted_fd = -1;
        pthread_mutex_unlock(&g_lock);
        g_api.screen_stop(state->screen);
        pthread_mutex_lock(&g_lock);
        state->screen_started = 0;
        pthread_mutex_unlock(&g_lock);
        return NULL;
    }

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "processing begin receiver=%p screen=%p delegate=%p",
             state->receiver, state->screen, delegate_context);

    rc = g_api.screen_process(state->screen, net, ALTSCREEN_TIMEOUT_SECS);

    pthread_mutex_lock(&g_lock);
    state->accepted_fd = -1;
    pthread_mutex_unlock(&g_lock);

    (void)g_api.netsock_delete(net);
    g_api.screen_stop(state->screen);

    pthread_mutex_lock(&g_lock);
    state->screen_started = 0;
    pthread_mutex_unlock(&g_lock);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "processing end receiver=%p rc=%d",
             state->receiver, rc);
    return NULL;
}

static int prepare_altscreen(void *receiver, cf_ref alt_desc, uint16_t *out_port)
{
    altscreen_state *state = NULL;
    void *screen = NULL;
    uint8_t key[16];
    uint8_t iv[16];
    uint64_t connection_id;
    int ok = 0;
    os_status rc;
    int listen_fd = -1;
    uint16_t port = 0;

    connection_id = (uint64_t)dict_get_i64(alt_desc, "streamConnectionID", &ok);
    if (!ok || connection_id == 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare refused reason=missing_connection_id");
        return 0;
    }

    if (!altscreen_aes_ready()) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare refused conn=%llu reason=stock_AES_not_ready primary_preserved=1",
                  (unsigned long long)connection_id);
        return 0;
    }

    pthread_mutex_lock(&g_lock);
    state = alloc_state_locked(receiver);
    pthread_mutex_unlock(&g_lock);
    if (state == NULL) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "prepare refused receiver=%p reason=state_collision_or_full",
                  receiver);
        return 0;
    }

    /* No worker/thread is added during normal boot.  Start only after a real
     * private Stream-111 SETUP allocated state for this receiver. */
    ensure_view_worker_started();

    rc = g_api.screen_create(&screen);
    if (rc != 0 || screen == NULL)
        goto fail;

    rc = g_api.screen_setup(screen, alt_desc, 0);
    if (rc != 0)
        goto fail;

    if (!altscreen_derive_aes(connection_id, key, iv))
        goto fail;
    rc = g_api.screen_security(screen, key, iv);
    memset(key, 0, sizeof(key));
    memset(iv, 0, sizeof(iv));
    if (rc != 0)
        goto fail;

    if (!altscreen_register_session(screen)) goto fail;

    listen_fd = open_listener(&port);
    if (listen_fd < 0)
        goto fail_registered;

    pthread_mutex_lock(&g_lock);
    state->screen = screen;
    state->connection_id = connection_id;
    state->listen_fd = listen_fd;
    state->data_port = port;
    pthread_mutex_unlock(&g_lock);

    rc = pthread_create(&state->worker, NULL, altscreen_worker, state);
    if (rc != 0) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "worker create failed receiver=%p rc=%d",
                  receiver, rc);
        goto fail_listener;
    }

    pthread_mutex_lock(&g_lock);
    state->worker_started = 1;
    pthread_mutex_unlock(&g_lock);

    *out_port = port;
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_SESSION_READY t_ms=%llu role=MAP receiver=%p screen=%p connection=%llu port=%u active=1 mapuuid=%s",
             (unsigned long long)now_ms(), receiver, screen,
             (unsigned long long)connection_id, (unsigned int)port, ALTSCREEN_UUID);
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_READY role=MAP receiver=%p screen=%p conn=%llu dataPort=%u primary_untouched=1",
             receiver, screen, (unsigned long long)connection_id,
             (unsigned int)port);
    return 1;

fail_listener:
    close(listen_fd);
    pthread_mutex_lock(&g_lock);
    state->listen_fd = -1;
    pthread_mutex_unlock(&g_lock);
fail_registered:
    altscreen_unregister_session(screen);
fail:
    if (screen != NULL)
        g_api.screen_delete(screen);
    pthread_mutex_lock(&g_lock);
    memset(state, 0, sizeof(*state));
    pthread_mutex_unlock(&g_lock);
    return 0;
}

static void cleanup_altscreen(void *receiver)
{
    altscreen_state *state;
    pthread_t worker;
    int worker_started;
    int listen_fd;
    int accepted_fd;
    uint16_t data_port;
    void *screen;
    int screen_started;
    int show_sent;
    int waiting_on_accept;
    int wake_ok = 1;

    pthread_mutex_lock(&g_lock);
    state = find_state_receiver_locked(receiver);
    if (state == NULL) {
        pthread_mutex_unlock(&g_lock);
        return;
    }
    worker = state->worker;
    worker_started = state->worker_started;
    listen_fd = state->listen_fd;
    accepted_fd = state->accepted_fd;
    data_port = state->data_port;
    screen = state->screen;
    screen_started = state->screen_started;
    show_sent = state->show_sent;
    state->stop_requested = 1;
    waiting_on_accept = worker_started && listen_fd >= 0 && accepted_fd < 0;
    if (accepted_fd >= 0)
        (void)shutdown(accepted_fd, SHUT_RDWR);
    pthread_mutex_unlock(&g_lock);

    if (show_sent)
        (void)send_event_to_uuid(receiver, "stopUI", NULL,
                        ALTSCREEN_UUID);
    if (screen_started && screen != NULL && g_api.screen_send_cmd != NULL)
        (void)g_api.screen_send_cmd(screen, 'q', NULL, 0);

    if (waiting_on_accept) {
        wake_ok = wake_listener(data_port);
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "listener wake receiver=%p port=%u ok=%d",
                 receiver, (unsigned int)data_port, wake_ok);
        if (!wake_ok) {
            (void)shutdown(listen_fd, SHUT_RDWR);
            (void)close(listen_fd);
            pthread_mutex_lock(&g_lock);
            if (state->listen_fd == listen_fd)
                state->listen_fd = -1;
            pthread_mutex_unlock(&g_lock);
            (void)pthread_cancel(worker);
        }
    }

    if (worker_started && !pthread_equal(worker, pthread_self()))
        (void)pthread_join(worker, NULL);

    /* A queued view update may still be finishing on the control worker.
     * Stop scheduling above, then give that bounded operation time to exit
     * before the receiver/session is handed back to stock teardown. */
    {
        int wait_i;
        for (wait_i = 0; wait_i < 50; ++wait_i) {
            int inflight;
            pthread_mutex_lock(&g_lock);
            inflight = state->control_inflight;
            pthread_mutex_unlock(&g_lock);
            if (inflight == 0)
                break;
            usleep(10000);
        }
    }

    pthread_mutex_lock(&g_lock);
    screen_started = state->screen_started;
    pthread_mutex_unlock(&g_lock);

    if (screen != NULL) {
        if (screen_started)
            g_api.screen_stop(screen);
        altscreen_unregister_session(screen);
        g_api.screen_delete(screen);
    }

    pthread_mutex_lock(&g_lock);
    memset(state, 0, sizeof(*state));
    pthread_mutex_unlock(&g_lock);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_CLEANUP role=MAP receiver=%p route_stop=%d wake_ok=%d",
             receiver, show_sent, wake_ok);
}
os_status k2161_output_session_setup(void *receiver,
                                      cf_ref request,
                                      cf_ref *out_response)
{
    cf_ref alt_desc = NULL;
    cf_ref stock_request = NULL;
    cf_ref stock_response = NULL;
    cf_ref final_response = NULL;
    uint16_t alt_port = 0;
    os_status rc;

    resolve_api();
    if (g_api.session_setup == NULL) return -1;
    /* OFF means exact stock arguments and exact stock response: never
     * advertise, allocate, intercept, change features or start MAP 111. */
    if (!altscreen_enabled() || !g_api.ready || out_response == NULL)
        return g_api.session_setup(receiver, request, out_response);

    log_setup_request(receiver, request);
    if (altscreen_map_foreign_stream111_present(request)) {
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "ALTSESSION_MAP_POLICY foreign_uuid=1 verdict=STOCK_PASSTHROUGH no_private_screen=1");
        return g_api.session_setup(receiver, request, out_response);
    }
    stock_request = build_stock_only_request(request, &alt_desc);
    if (stock_request == NULL || alt_desc == NULL) {
        /* When no MAP descriptor exists, preserve the pre-AltScreen supported
         * feature response for the enabled MAP configuration only. */
        rc = g_api.session_setup(receiver, request, &stock_response);
        if (rc == 0 && stock_response != NULL) {
            final_response = clone_response_with_reference_features(stock_response);
            if (final_response != NULL) {
                g_api.release(stock_response);
                stock_response = final_response;
            }
        }
        *out_response = stock_response;
        log_setup_response_features(receiver, rc, stock_response);
        return rc;
    }
    /* Only the MAP UUID is accepted. An unexpected second Type-111
     * descriptor is never taken over by our private output. */
    if (altscreen_map_map_uuid_role(alt_desc) == 0) {
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "ALTSESSION_MAP_UUID_POLICY method=single_111_no_uuid_fallback exact_match=0 reason=missing_uuid");
    } else {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,"ALTSESSION_MAP_UUID_POLICY method=exact_uuid exact_match=1");
    }
    rc = g_api.session_setup(receiver, stock_request, &stock_response);
    log_setup_response_features(receiver, rc, stock_response);
    g_api.release(stock_request);
    if (rc != 0) {
        if (stock_response) g_api.release(stock_response);
        return rc;
    }
    if (!prepare_altscreen(receiver, alt_desc, &alt_port)) {
        *out_response = stock_response;
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "ALTSESSION_MAP prepare failed receiver=%p stock_response_preserved=1",receiver);
        return 0;
    }
    final_response = merge_altscreen_response(stock_response, alt_desc, alt_port);
    if (final_response == NULL) {
        cleanup_altscreen(receiver);
        *out_response = stock_response;
        LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                  "ALTSESSION_MAP merge failed receiver=%p stock_response_preserved=1",receiver);
        return 0;
    }
    if (stock_response) g_api.release(stock_response);
    *out_response = final_response;
    LOG_WARN(ALTSCREEN_SESSION_MODULE,
             "ALTSESSION_MAP_SETUP_READY receiver=%p map_port=%u enabledFeatures=altScreen|viewAreas",
             receiver, (unsigned int)alt_port);
    return 0;
}

void k2161_output_session_teardown(void *receiver,
                                    cf_ref request,
                                    os_status reason,
                                    unsigned char *out_done)
{
    int has_alt = 0;
    int has_main = 0;
    int other_streams = 0;
    int full_session;
    cf_ref stock_request = NULL;
    cf_ref ignored_alt = NULL;

    resolve_api();
    if (g_api.session_teardown == NULL)
        return;

    if (!altscreen_enabled() || !g_api.ready) {
        g_api.session_teardown(receiver, request, reason, out_done);
        return;
    }

    full_session = request == NULL;
    if (!full_session && altscreen_map_foreign_stream111_present(request)) {
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "ALTSESSION_MAP_POLICY teardown_foreign_uuid=1 verdict=STOCK_PASSTHROUGH");
        g_api.session_teardown(receiver, request, reason, out_done);
        return;
    }
    classify_teardown_request(request, &has_alt, &has_main, &other_streams);

    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "TEARDOWN receiver=%p request=%s reason=%d alt111=%d main110=%d other=%d private111_decoupled=1",
             receiver, full_session ? "NULL" : "dict", reason,
             has_alt, has_main, other_streams);

    /*
     * Stream 111 is private to this hook.  A teardown for stock 110, alt audio
     * (101), or any other stock stream must never destroy the private screen.
     * The previous build cleaned up 111 for every teardown; the car trace shows
     * exactly such a stock teardown immediately before dio_manager faulted.
     */
    if (!full_session && !has_alt) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "TEARDOWN preserving private111 reason=stock_only_or_other_stream");
        g_api.session_teardown(receiver, request, reason, out_done);
        return;
    }

    if (has_alt) {
        LOG_INFO(ALTSCREEN_SESSION_MODULE,
                 "TEARDOWN explicit111 cleanup other_streams=%d", other_streams);
        cleanup_altscreen(receiver);

        /* Stock never saw stream 111 during SETUP, so never pass 111 back to
         * stock teardown.  If 111 was the only requested stream, we are done. */
        if (other_streams == 0) {
            if (out_done != NULL)
                *out_done = 0;
            return;
        }

        stock_request = build_stock_only_request(request, &ignored_alt);
        if (stock_request != NULL) {
            g_api.session_teardown(receiver, stock_request, reason, out_done);
            g_api.release(stock_request);
        } else {
            LOG_ERROR(ALTSCREEN_SESSION_MODULE,
                      "TEARDOWN explicit111 split failed; preserving stock request fail-safe");
            g_api.session_teardown(receiver, request, reason, out_done);
        }
        return;
    }

    /* A true whole-session teardown owns private111 cleanup. */
    LOG_INFO(ALTSCREEN_SESSION_MODULE,
             "TEARDOWN whole-session private111 cleanup");
    cleanup_altscreen(receiver);
    g_api.session_teardown(receiver, request, reason, out_done);
}

os_status k2161_output_screen_data(void *stream,
                                  const uint8_t *data,
                                  size_t len,
                                  uint64_t display_ticks,
                                  cf_ref options,
                                  void (*completion)(void *),
                                  void *completion_context)
{
    os_status rc;
    altscreen_state *state = NULL;
    pthread_t self;
    int should_show = 0;

    resolve_api();
    if (g_api.screen_process_data == NULL)
        return -1;

    rc = g_api.screen_process_data(stream, data, len, display_ticks,
                                   options, completion, completion_context);
    if (rc != 0 || !altscreen_enabled())
        return rc;

    self = pthread_self();
    pthread_mutex_lock(&g_lock);
    state = find_state_worker_locked(self);
    if (state != NULL && state->screen_started && !state->show_sent &&
        !state->stop_requested) {
        state->show_sent = 1;
        should_show = 1;
    }
    pthread_mutex_unlock(&g_lock);

    if (should_show) {
        LOG_WARN(ALTSCREEN_SESSION_MODULE,
                 "FIRST_REAL_FRAME receiver=%p stream=%p len=%u route=cluster",
                 state->receiver, stream, (unsigned int)len);
        request_cluster(state);
    }
    return rc;
}
