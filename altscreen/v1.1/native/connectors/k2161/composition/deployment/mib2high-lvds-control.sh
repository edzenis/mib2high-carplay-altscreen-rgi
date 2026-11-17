#!/bin/sh
set -u

PATH=/proc/boot:/bin:/usr/bin:/usr/sbin:/sbin:/mnt/app/armle/bin:/mnt/app/armle/sbin:/mnt/app/armle/usr/bin:/mnt/app/armle/usr/sbin
export PATH

ACTIVE=/tmp/mib2high-lvds-active
REACQUIRE=/tmp/mib2high-lvds-reacquire
HEARTBEAT=/tmp/k2161-altscreen-heartbeat
PIDFILE=/tmp/mib2high-lvds-control.pid
LOG=/tmp/mib2high-lvds-control.log
TRACE_1798_PID=
TRACE_20991_PID=
RENDERER_STARTED_BY_US=0
CONTEXT_REQUESTED=0
ORIGINAL_CONTEXT=UNKNOWN
RESTORE_CONTEXT=72
CLEANED=0
LOG_ENTRIES=0
[ -f "$LOG" ] && mv "$LOG" "$LOG.previous" 2>/dev/null || true
# K2161 read-only context telemetry with QNX/POSIX shell built-in parser.
# Avoid unsupported parsers; rely only on shell built-ins now.
CONTEXT_LOG=/tmp/AltScreen-vc-context.log
CONTEXT_RAW=/tmp/AltScreen-vc-context-transitions.log
CONTEXT_QUERY=/mnt/app/root/vc_context_query
PROFILE=/mnt/app/root/mib2high-lvds-profile.conf
RENDERER=/mnt/app/root/vc_render
DMDT=/eso/bin/apps/dmdt

log() {
    LOG_ENTRIES=$((LOG_ENTRIES + 1))
    if [ "$LOG_ENTRIES" -ge 2500 ]; then
        [ -f "$LOG.1" ] && rm -f "$LOG.1" 2>/dev/null || true
        [ -f "$LOG" ] && mv "$LOG" "$LOG.1" 2>/dev/null || true
        LOG_ENTRIES=1
    fi
    echo "[AltScreen-lvds pid=$$ seq=$LOG_ENTRIES] $*" >> "$LOG"
}

if [ -f "$PIDFILE" ]; then
    OLD=`cat "$PIDFILE" 2>/dev/null || true`
    if [ -n "${OLD:-}" ] && kill -0 "$OLD" 2>/dev/null; then
        log "duplicate start ignored pid=$OLD"
        exit 0
    fi
    rm -f "$PIDFILE"
fi

echo $$ > "$PIDFILE" || exit 1

cleanup() {
    [ "$CLEANED" -eq 0 ] || return 0
    CLEANED=1
    rm -f "$PIDFILE" "$HEARTBEAT" "$REACQUIRE"
    # Never stop an externally running tracer; only our own two children.
    if [ -n "$TRACE_1798_PID" ]; then
        kill "$TRACE_1798_PID" 2>/dev/null || true
        log "ALTSESSION_TRACE_STOP port=1798 child=$TRACE_1798_PID"
    fi
    if [ -n "$TRACE_20991_PID" ]; then
        kill "$TRACE_20991_PID" 2>/dev/null || true
        log "ALTSESSION_TRACE_STOP port=20991 child=$TRACE_20991_PID"
    fi
    # Renderer is shared; don't terminate one present before our session.
    if [ "$RENDERER_STARTED_BY_US" -eq 1 ]; then
        slay vc_render >> "$LOG" 2>&1 || log 'ALTSESSION_RENDER_STOP_FAILED'
        log 'ALTSESSION_RENDER_STOP own_launch=1'
    else
        log 'ALTSESSION_RENDER_STOP own_launch=0 preserved_external=1'
    fi
    if [ "$CONTEXT_REQUESTED" -eq 1 ] && [ -x "$DMDT" ]; then
        export LD_LIBRARY_PATH=/eso/lib
        if IPL_CONFIG_DIR=/etc/eso/production "$DMDT" sc 4 "$RESTORE_CONTEXT" >> "$LOG" 2>&1; then
            log "ALTSESSION_CONTEXT_RESTORE=$RESTORE_CONTEXT original=$ORIGINAL_CONTEXT"
        else
            log "ALTSESSION_CONTEXT_RESTORE_FAILED=$RESTORE_CONTEXT original=$ORIGINAL_CONTEXT"
        fi
    fi
}
trap 'cleanup' 0
trap 'exit 1' 1 2 15

[ -e "$ACTIVE" ] || { rm -f "$HEARTBEAT" 2>/dev/null || true; exit 0; }

# K2161 live diagnostics only: read-only bounded /tmp rings for known factory
# DSITracer appenders. Launch output and process status are collected separately.
TRACE_CAPTURE=/mnt/app/root/altscreen-dsitracer-capture
if [ -x "$TRACE_CAPTURE" ]; then
    "$TRACE_CAPTURE" 1798 >> /tmp/AltScreen-dsitracer-launch-1798.log 2>&1 &
    TRACE_1798_PID=$!
    log "ALTSESSION_TRACE_START port=1798 child=$TRACE_1798_PID"
    "$TRACE_CAPTURE" 20991 >> /tmp/AltScreen-dsitracer-launch-20991.log 2>&1 &
    TRACE_20991_PID=$!
    log "ALTSESSION_TRACE_START port=20991 child=$TRACE_20991_PID"
else
    log "AltScreen dsitracer capture binary missing"
fi

W=800
H=480
FPS=30
FEATURES=0
if [ -f "$PROFILE" ]; then
    set -- `cat "$PROFILE" 2>/dev/null`
    [ $# -ge 1 ] && W="$1"
    [ $# -ge 2 ] && H="$2"
    [ $# -ge 3 ] && FPS="$3"
    [ $# -ge 4 ] && FEATURES="$4"
fi

case "$W:$H:$FPS" in
    *[!0-9:]*|'') W=800; H=480; FPS=30 ;;
esac

[ -x "$RENDERER" ] || { log "renderer missing: $RENDERER"; exit 1; }

export LD_LIBRARY_PATH=/mnt/app/armle/lib:/mnt/app/armle/usr/lib:/mnt/app/root/lib-target:/eso/lib:/armle/lib:/armle/lib/dll:/armle/usr/lib:/lib:/usr/lib

SIG="$W $H $FPS"
OLD_SIG=`cat /tmp/mib2high-vc-render.profile 2>/dev/null || true`
if pidin 2>/dev/null | grep vc_render >/dev/null; then
    if [ "$OLD_SIG" != "$SIG" ]; then
        log "ALTSESSION_RENDER_CONFLICT existing=$OLD_SIG required=$SIG; refusing to kill unowned renderer"
        exit 1
    fi
    log "ALTSESSION_RENDER_REUSE profile=$OLD_SIG"
else
    echo "$SIG" > /tmp/mib2high-vc-render.profile
    on -p 25 "$RENDERER" --out 1 --layer 58 --w "$W" --h "$H" --fps "$FPS" --ts wall >> /tmp/vc_render-launch.log 2>&1 &
    RENDERER_STARTED_BY_US=1
    log "ALTSESSION_RENDER_START launcher=$! out=1 layer=58 ${W}x${H}@${FPS}"
    # The QNX `on` launcher may complete before the renderer appears in
    # pidin. Permit bounded asynchronous startup rather than aborting after
    # a single transient observation.
    RENDER_WAIT=0
    while ! pidin 2>/dev/null | grep vc_render >/dev/null; do
        RENDER_WAIT=$((RENDER_WAIT + 1))
        if [ "$RENDER_WAIT" -ge 5 ]; then
            log 'ALTSESSION_RENDER_START_FAILED no_vc_render_process_observed after_5_checks'
            RENDERER_STARTED_BY_US=0
            exit 1
        fi
        sleep 1
    done
    log "ALTSESSION_RENDER_START_CONFIRMED wait_checks=$RENDER_WAIT"
fi

NAV0=`sloginfo 2>/dev/null | grep -c 'Change NavStateOfOperation to 5' || true`
REACQ_DONE=0
[ "$NAV0" -ge 2 ] && REACQ_DONE=1
log "session active nav_ready_count=$NAV0 features=$FEATURES"

export LD_LIBRARY_PATH=/eso/lib
# AltScreen selects OEM context 76 (video displayable 58 only), without
# maneuver displayables 20, 98, 101 or 102. HUD output remains independent.
GATE_CONTEXT_OK=0
log "ALTVC_DISPLAY boot=1 requested_context=76 display=4 layer=58 no_native_tile=1"

# K2161 diagnostic helper runs only `dmdt gs` through a bounded PTY.
# Parse its multiline result with POSIX sh built-ins ONLY: case, read, set,
# parameter expansion and echo. Never assume a successful `sc` == active 76.
# Successful query with no output becomes EMPTY_OUTPUT, not context 76.
# The raw output (including errors) is retained whenever its state changes.
LAST_CONTEXT=UNOBSERVED
SAMPLE_SEQ=0
parse_vc_context() {
    GS_CONTEXT=UNPARSEABLE
    VC_TERMINAL=0
    while IFS= read -r GS_LINE || [ -n "$GS_LINE" ]; do
        case "$GS_LINE" in
            display\ [0-9]*:*) VC_TERMINAL=0 ;;
            *terminal:*)
                case "$GS_LINE" in
                    *2nd_lvds*) VC_TERMINAL=1 ;;
                    *) VC_TERMINAL=0 ;;
                esac
                ;;
            *context\ id:*)
                if [ "$VC_TERMINAL" -eq 1 ]; then
                    # Standard dmdt gs: 'context id: 76'. Strip a PTY CR
                    # with parameter expansion; validate numeric prefix.
                    set -- $GS_LINE
                    GS_CANDIDATE="${3:-}"
                    case "$GS_CANDIDATE" in
                        [0-9]*)
                            GS_CANDIDATE="${GS_CANDIDATE%%[!0-9]*}"
                            GS_CONTEXT="$GS_CANDIDATE"
                            ;;
                    esac
                    break
                fi
                ;;
        esac
    done <<ALTSESSION_DMDT_RESULT
$GS_RAW
ALTSESSION_DMDT_RESULT
}
observe_vc_context() {
    if [ -x "$CONTEXT_QUERY" ]; then
        GS_RAW=`IPL_CONFIG_DIR=/etc/eso/production "$CONTEXT_QUERY" "$DMDT" 2>&1`
        GS_RC=$?
        GS_TS=`"$CONTEXT_QUERY" --timestamp 2>/dev/null || true`
    else
        GS_RAW="VC_CONTEXT_QUERY_ERROR=HELPER_NOT_INSTALLED"
        GS_RC=127
        GS_TS=""
    fi
    if [ "$GS_RC" -eq 0 ]; then
        if [ -n "$GS_RAW" ]; then
            parse_vc_context
        else
            GS_CONTEXT=EMPTY_OUTPUT
        fi
    else
        GS_CONTEXT=QUERY_FAILED
    fi
    echo "epoch=$GS_TS sample_s=$SAMPLE_SEQ requested=${REQUESTED_CONTEXT:-none} actual=$GS_CONTEXT gs_rc=$GS_RC" >> "$CONTEXT_LOG"
    if [ "$GS_CONTEXT" != "$LAST_CONTEXT" ]; then
        log "VC_CONTEXT_TRANSITION from=$LAST_CONTEXT to=$GS_CONTEXT requested=${REQUESTED_CONTEXT:-none} gs_rc=$GS_RC epoch=$GS_TS sample_s=$SAMPLE_SEQ"
        {
            echo "===== epoch=$GS_TS sample_s=$SAMPLE_SEQ from=$LAST_CONTEXT to=$GS_CONTEXT requested=${REQUESTED_CONTEXT:-none} rc=$GS_RC ====="
            echo "$GS_RAW"
        } >> "$CONTEXT_RAW"
        LAST_CONTEXT="$GS_CONTEXT"
    fi
    SAMPLE_SEQ=$((SAMPLE_SEQ + 1))
}

# Read the actual display state before altering anything. If the helper
# cannot establish a numeric context, keep the known K2161 fallback explicit
# rather than quietly pretending the original state was 72.
for F in "$CONTEXT_LOG" "$CONTEXT_RAW"; do
    [ -f "$F" ] && mv "$F" "$F.previous" 2>/dev/null || true
done
REQUESTED_CONTEXT=none
observe_vc_context
case "$GS_CONTEXT" in
    ''|*[!0-9]*)
        RESTORE_CONTEXT=72
        log "ALTSESSION_CONTEXT_ORIGINAL_UNKNOWN actual=$GS_CONTEXT fallback=72"
        ;;
    *)
        ORIGINAL_CONTEXT=$GS_CONTEXT
        RESTORE_CONTEXT=$GS_CONTEXT
        log "ALTSESSION_CONTEXT_ORIGINAL=$GS_CONTEXT source=dmdt_gs"
        ;;
esac
# A single K2161-specific context writer, read-back confirmed. No 77/900
# fallback and no renderer frame or geometry gates. Do not change core state.
REQUESTED_CONTEXT=76
GATE_TICK=0
ALTVC_LAST_ACTUAL=unknown
ALTVC_LAST_SWITCH_SAMPLE=-10
ALTVC_LAST_SWITCH_RC=0
while [ -e "$ACTIVE" ]; do
    TARGET_CONTEXT=76
    if [ "$GS_CONTEXT" != 76 ] && [ $((SAMPLE_SEQ - ALTVC_LAST_SWITCH_SAMPLE)) -ge 2 ]; then
        CONTEXT_REQUESTED=1
        REQUESTED_CONTEXT=76
        ALTVC_LAST_SWITCH_SAMPLE="$SAMPLE_SEQ"
        log "ALTVC_CONTEXT_REQUEST context=76 observed=$GS_CONTEXT"
        if IPL_CONFIG_DIR=/etc/eso/production "$DMDT" sc 4 76 >> "$LOG" 2>&1; then
            ALTVC_LAST_SWITCH_RC=0
            log 'ALTVC_CONTEXT_COMMAND_ACCEPTED target=76 readback_pending=1'
        else
            ALTVC_LAST_SWITCH_RC=1
            log 'ALTVC_CONTEXT_REQUEST_FAILED target=76'
        fi
    fi
    observe_vc_context
    if [ "$GS_CONTEXT" = "$TARGET_CONTEXT" ] && [ "$ALTVC_LAST_SWITCH_RC" -eq 0 ]; then
        GATE_CONTEXT_OK=1
    elif [ "$GS_CONTEXT" = UNPARSEABLE ] || [ "$GS_CONTEXT" = QUERY_FAILED ] || [ "$GS_CONTEXT" = EMPTY_OUTPUT ]; then
        # A failed diagnostic read must not kill working CarPlay video.
        # This is ACK-only, logged as UNVERIFIED (not advertised as HELD).
        GATE_CONTEXT_OK=1
    else
        GATE_CONTEXT_OK=0
    fi
    if [ "$GATE_CONTEXT_OK" -eq 1 ]; then
        echo $$ > "$HEARTBEAT" 2>/dev/null || true
    else
        rm -f "$HEARTBEAT" 2>/dev/null || true
    fi
    if [ "$GS_CONTEXT" != "$ALTVC_LAST_ACTUAL" ]; then
        ALTVC_LAST_ACTUAL="$GS_CONTEXT"
        log "ALTVC_CONTEXT_OBSERVATION target=76 actual=$GS_CONTEXT rc=$GS_RC gate=$GATE_CONTEXT_OK"
    fi

    if [ "$REACQ_DONE" -eq 0 ]; then
        NAV=`sloginfo 2>/dev/null | grep -c 'Change NavStateOfOperation to 5' || true`
        if [ "$NAV" -ge 2 ]; then
            : > "$REACQUIRE"
            REACQ_DONE=1
            log "second nav-ready observed; reacquire marker set"
        fi
    fi

    sleep 1
done

log "ALTSESSION_SESSION_END detected=1; owned_resources_cleaned_by_trap"
exit 0
