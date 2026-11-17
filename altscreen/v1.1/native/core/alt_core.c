#include "alt_core.h"
#include <string.h>
void alt_core_reset(alt_core_state *s) {
    if (!s) return;
    memset(s,0,sizeof(*s));
    s->current_view=-1; s->pending_view=-1; s->control_target_view=-1; s->last_ui_slot=-1;
    s->pending_presentation=-1; s->control_target_presentation=-1;
}
int alt_core_update_route(alt_core_state *s,int active) {
    int changed = !s->route_known || s->route_active!=active;
    s->route_active=active; s->route_known=1; return changed;
}
void alt_core_queue_view(alt_core_state *s,int view,int presentation_id,int gauge,uint64_t due) {
    s->pending_view=view; s->pending_presentation=presentation_id; s->pending_view_from_gauge=gauge; s->view_due_ms=due;
}
int alt_core_promote_view(alt_core_state *s,uint64_t now) {
    int desired;
    if (s->pending_view<0 || s->view_due_ms>now) return 0;
    desired=s->pending_view;
    s->control_target_view=desired;
    s->control_target_presentation=s->pending_presentation;
    s->pending_presentation=-1;
    s->control_target_from_gauge=s->pending_view_from_gauge;
    s->pending_view=-1; s->pending_view_from_gauge=0; s->view_due_ms=0;
    s->control_stage=(s->last_ui_slot!=s->control_target_presentation)?ALT_CORE_SHOW:ALT_CORE_VIEW;
    return 1;
}
int alt_core_timeout(alt_core_state *s,uint64_t now) {
    if (!s->control_awaiting_ack || now<s->control_deadline_ms) return 0;
    s->control_awaiting_ack=0; s->control_deadline_ms=0;
    if (s->control_stage==ALT_CORE_SHOW) s->last_ui_slot=-1;
    s->control_stage=ALT_CORE_IDLE; s->control_startup=0;
    return 1;
}
int alt_core_ack(alt_core_state *s,uint64_t token,int stage,int view,int status) {
    if (!s->control_awaiting_ack || s->control_token!=token || s->control_stage!=stage) return 0;
    s->control_awaiting_ack=0; s->control_deadline_ms=0;
    if (status==0) {
        if(stage==ALT_CORE_SHOW){s->last_ui_slot=s->control_target_presentation;s->control_stage=ALT_CORE_VIEW;}
        else if(stage==ALT_CORE_VIEW)s->control_stage=ALT_CORE_KEYFRAME;
        else{s->current_view=view;s->control_stage=ALT_CORE_IDLE;s->control_startup=0;}
    } else {
        if(stage==ALT_CORE_SHOW)s->last_ui_slot=-1;
        s->control_stage=ALT_CORE_IDLE;s->control_startup=0;
    }
    return 1;
}
int alt_core_dispatch_failed(alt_core_state *s,uint64_t token,int stage) {
    if(!s->control_awaiting_ack || s->control_token!=token || s->control_stage!=stage) return 0;
    s->control_awaiting_ack=0;s->control_deadline_ms=0;s->control_stage=ALT_CORE_IDLE;s->control_startup=0;
    if(stage==ALT_CORE_SHOW)s->last_ui_slot=-1;
    return 1;
}
int alt_core_zoom_enqueue(alt_core_zoom_state *s,int step,uint64_t source_id) {
    if(!s || step==0 || s->pending<=-20 || s->pending>=20) return 0;
    s->pending+=(step>0)?1:-1;
    s->source_id=source_id;
    return 1;
}
void alt_core_zoom_plan_next(alt_core_zoom_state *s,int has_target,int display_active,uint64_t now,alt_core_zoom_plan *out) {
    if(!out)return;
    memset(out,0,sizeof(*out));
    if(!s || s->pending==0)return;
    if(has_target && display_active) {
        out->result=ALT_ZOOM_SEND;
        out->step=s->pending>0?1:-1;
        s->pending-=out->step;
        out->remaining=s->pending;
        out->source_id=s->source_id;
        out->dispatch_id=++s->next_dispatch_id;
    } else if(!display_active) {
        out->result=ALT_ZOOM_DROP;
        out->dropped=s->pending;
        s->pending=0;
    } else {
        out->result=ALT_ZOOM_BLOCKED;
        if(s->last_blocked_ms==0 || now-s->last_blocked_ms>=1000) {
            out->report_blocked=1;
            s->last_blocked_ms=now;
        }
        out->remaining=s->pending;
    }
}
int alt_core_input_event(alt_core_state *s,alt_core_input e) {
    if(!s)return 0;
    if(e.kind==ALT_INPUT_ROUTE) {
        int changed=alt_core_update_route(s,e.value!=0);
        alt_core_queue_view(s,e.view_id,e.presentation_id,0,e.now_ms+e.delay_ms);
        return changed;
    }
    if(e.kind==ALT_INPUT_LAYOUT) {
        alt_core_queue_view(s,e.view_id,e.presentation_id,1,e.now_ms+e.delay_ms);
        return 1;
    }
    if(e.kind==ALT_INPUT_START_VIDEO) {
        alt_core_queue_view(s,e.view_id,e.presentation_id,0,e.now_ms);
        s->control_startup=1;
        return 1;
    }
    return 0;
}

int alt_core_session_ready(int connected,int video_started,int stopping) {
    return connected && video_started && !stopping;
}
int alt_core_make_plan(alt_core_state *s,uint64_t now,int can_display,uint64_t next_token,alt_core_plan *out) {
    if(!s || !out) return 0;
    memset(out,0,sizeof(*out));
    out->old_target=s->control_target_view;
    out->desired_view=s->pending_view;
    if(s->control_awaiting_ack && now>=s->control_deadline_ms) {
        out->timeout=1;
        (void)alt_core_timeout(s,now);
    }
    if(s->control_awaiting_ack)return 0;
    if(s->pending_view>=0) {
        if(s->view_due_ms>now) {out->deferred=1;return 0;}
        out->promoted=alt_core_promote_view(s,now);
    }
    if(s->control_stage==ALT_CORE_IDLE || (!can_display && !s->control_startup))return 0;
    out->dispatched=1;
    out->stage=s->control_stage;out->view=s->control_target_view;out->token=next_token;
    s->control_token=next_token;
    s->control_awaiting_ack=1;
    s->control_deadline_ms=now+ALT_CORE_ACK_TIMEOUT_MS;
    return 1;
}
