#ifndef MIB2HIGH_OUTPUT_SCOPE_H
#define MIB2HIGH_OUTPUT_SCOPE_H
/* Private per-thread MAP NvSS interception scope.
 * Unlike the former process-global thread owner, each auxiliary Screen
 * session can StartSession without touching the stock primary screen.
 * Outside the scoped stock StartSession call, NvSS hooks pass through.
 * QNX 6.5 pthread TLS is used instead of a process-global role.
 */
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    unsigned int depth;
} altscreen_scope_t;
static pthread_once_t alt_scope_once = PTHREAD_ONCE_INIT;
static pthread_key_t alt_scope_key;
static int alt_scope_key_result = -1;

static void alt_scope_destroy(void *p) { free(p); }
static void alt_scope_make_key(void)
{
    alt_scope_key_result = pthread_key_create(&alt_scope_key, alt_scope_destroy);
}
static altscreen_scope_t *alt_scope_current(void)
{
    if (pthread_once(&alt_scope_once, alt_scope_make_key) != 0 ||
        alt_scope_key_result != 0)
        return NULL;
    return (altscreen_scope_t *)pthread_getspecific(alt_scope_key);
}
static int altscreen_scope_begin(void)
{
    altscreen_scope_t *s = alt_scope_current();
    if (alt_scope_key_result != 0) return 0;
    if (!s) {
        s = (altscreen_scope_t *)calloc(1, sizeof(*s));
        if (!s) return 0;
        if (pthread_setspecific(alt_scope_key, s) != 0) {
            free(s);
            return 0;
        }
    }
    /* Nested calls on the same thread must not silently change the role. */
    if (s->depth == (unsigned int)-1) return 0;
    ++s->depth;
    return 1;
}
static void altscreen_scope_end(void)
{
    altscreen_scope_t *s = alt_scope_current();
    if (s && s->depth) {
        --s->depth;
    }
}
static int altscreen_scope_active(void)
{
    altscreen_scope_t *s = alt_scope_current();
    return s && s->depth != 0;
}

#endif
