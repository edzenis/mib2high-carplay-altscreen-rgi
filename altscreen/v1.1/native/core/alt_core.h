/* AltScreen portable policy engine: no operating-system, AirPlay, guidance transport or MMI dependency. */
#ifndef ALTSCREEN_CORE_H
#define ALTSCREEN_CORE_H
#include <stdint.h>
#define ALT_CORE_IDLE 0
#define ALT_CORE_SHOW 1
#define ALT_CORE_VIEW 2
#define ALT_CORE_KEYFRAME 3
#define ALT_CORE_ACK_TIMEOUT_MS 2500ULL
#define ALT_CORE_VIEW_DELAY_MS 150ULL
typedef struct {
    int route_known, route_active;
    int current_view, pending_view, pending_view_from_gauge;
    int pending_presentation, control_target_presentation;
    uint64_t view_due_ms;
    int last_ui_slot; /* neutral presentation identity, not view_id/2 */
    int control_stage, control_target_view, control_target_from_gauge;
    int control_awaiting_ack, control_startup;
    uint64_t control_token, control_deadline_ms;
} alt_core_state;
/* Firmware-neutral input events and output-plan data. Input connectors normalize
 * MMI/CarPlay events into these values; output connectors execute plans. */
typedef enum {
    ALT_INPUT_ROUTE=1, ALT_INPUT_LAYOUT=2, ALT_INPUT_START_VIDEO=3
} alt_core_input_kind;
typedef struct {
    alt_core_input_kind kind;
    int value;  /* route active, layout ID, or initial view ID */
    int view_id; /* output-connector-defined view identifier; no arithmetic in core */
    int presentation_id; /* connector-defined showUI identity shared by views */
    uint64_t now_ms;
    uint64_t delay_ms;
} alt_core_input;
typedef struct {
    int timeout, promoted, deferred, dispatched, old_target, desired_view;
    int stage, view;
    uint64_t token;
} alt_core_plan;
/* The connector supplies view/presentation identity; core only arbitrates them. */
__attribute__((visibility("hidden"))) int alt_core_input_event(alt_core_state *s,alt_core_input e);
/* Choose next output command and record its ACK/deadline state atomically.
 * Caller holds the session lock and supplies the abstract display eligibility. */
/* Shared session eligibility, with native receiver/lifecycle translated into booleans. */
__attribute__((visibility("hidden"))) int alt_core_session_ready(int connected, int video_started, int stopping);
__attribute__((visibility("hidden"))) int alt_core_make_plan(alt_core_state *s,uint64_t now,int can_display,uint64_t next_token,alt_core_plan *out);
__attribute__((visibility("hidden"))) void alt_core_reset(alt_core_state *s);
__attribute__((visibility("hidden"))) int alt_core_update_route(alt_core_state *s, int active);
__attribute__((visibility("hidden"))) void alt_core_queue_view(alt_core_state *s, int view, int presentation_id, int from_gauge, uint64_t due);
/* Return 1 when latest desired view was promoted into the command sequence. */
__attribute__((visibility("hidden"))) int alt_core_promote_view(alt_core_state *s, uint64_t now);
/* Return 1 if timeout invalidated an outstanding command. */
__attribute__((visibility("hidden"))) int alt_core_timeout(alt_core_state *s, uint64_t now);
/* Return 1 only for a command belonging to the current generation/token/stage. */
__attribute__((visibility("hidden"))) int alt_core_ack(alt_core_state *s, uint64_t token, int stage, int view, int status);
__attribute__((visibility("hidden"))) int alt_core_dispatch_failed(alt_core_state *s, uint64_t token, int stage);
/* Zoom queue arbitration is shared policy. A connector only chooses whether an
 * eligible target/active display exists, and sends the resulting zoom action. */
typedef struct {
    int pending;
    uint64_t source_id, next_dispatch_id, last_blocked_ms;
} alt_core_zoom_state;
typedef enum { ALT_ZOOM_NONE=0, ALT_ZOOM_SEND=1, ALT_ZOOM_DROP=2, ALT_ZOOM_BLOCKED=3 } alt_core_zoom_result;
typedef struct {
    alt_core_zoom_result result;
    int step, remaining, dropped;
    uint64_t source_id, dispatch_id;
    int report_blocked;
} alt_core_zoom_plan;
__attribute__((visibility("hidden"))) int alt_core_zoom_enqueue(alt_core_zoom_state *s, int step, uint64_t source_id);
__attribute__((visibility("hidden"))) void alt_core_zoom_plan_next(alt_core_zoom_state *s, int has_target, int display_active, uint64_t now_ms, alt_core_zoom_plan *out);
#endif
