#!/bin/sh
set -u
PATH=/bin:/usr/bin:/sbin:/usr/sbin:$PATH; export PATH
SD="${MIB_SD:-/net/mmx/fs/sda0}"
APP="${MIB_APP:-/mnt/app}"
SYSTEM="${MIB_SYSTEM:-/mnt/system}"
PKGNAME=mib2high-carplay-rgi-v1.1
MOD="$SD/mod/$PKGNAME"; PAY="$MOD/payload"; BACKUP="$MOD/backup/pre-v1.1"
INSTALLED="$MOD/installed.ok"; ROLLBACK="$MOD/ROLLBACK"
PENDING="$MOD/installing.flag"; COLLECT="$MOD/collected"
say(){ echo "[AltScreen-v1.1] $*"; }
fail(){ say "ERROR $*"; exit 1; }
# Same write test / guarded mount -uw logic as the working AltScreen simple installer.
checkrw() {
    MOUNTPOINT="$1"
    TESTFILE="$MOUNTPOINT/.mib2high-rgi-rw-test.$$"
    if ( : > "$TESTFILE" ) 2>/dev/null; then
        rm -f "$TESTFILE" 2>/dev/null
        return 0
    fi
    say "Remounting $MOUNTPOINT read-write..."
    mount -uw "$MOUNTPOINT" >/dev/null 2>&1 || return 1
    if ( : > "$TESTFILE" ) 2>/dev/null; then
        rm -f "$TESTFILE" 2>/dev/null
        return 0
    fi
    return 1
}
# All touched paths are indexed. Legacy marker is *removed* on install only
# to clear older builds, and restored on rollback if previously present.
NAMES='libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar maneuver_gl flag_atlas.rgba vc_render mib2high-lvds-control.sh mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf bundles_dsitracer_enabled.properties traceConfig.properties smartphone_integrator.json carplay_altscreen mib2high-altscreen-switch.conf dsitracer_activated altscreen-dsitracer-capture vc_context_query'
pathof(){ case "$1" in
libmib2high-carplay-rgi.so|flag_atlas.rgba) echo "$APP/root/hooks/$1";;
mib2high-carplay-rgi.jar) echo "$APP/eso/hmi/lsd/jars/$1";;
vc_render|maneuver_gl|altscreen-dsitracer-capture|vc_context_query|mib2high-lvds-control.sh|mib2high-lvds-profile.conf|mib2high-lvds-viewareas.conf) echo "$APP/root/$1";;
bundles_dsitracer_enabled.properties|traceConfig.properties|dsitracer_activated) echo "$APP/eso/hmi/lsd/$1";;
smartphone_integrator.json) echo "$SYSTEM/etc/eso/production/smartphone_integrator.json";;
carplay_altscreen) echo "$APP/carplay_altscreen";;
mib2high-altscreen-switch.conf) echo "$APP/root/mib2high-altscreen-switch.conf";; esac; }
modeof(){ case "$1" in *.so|vc_render|maneuver_gl|altscreen-dsitracer-capture|vc_context_query|*.sh) echo 755;;*)echo 644;;esac; }
# Preserve actual pre-install POSIX permissions (including setuid/setgid/sticky).
# Only uses QNX-standard ls and awk; no stat -c, dirname, cmp or other GNU options.
filemode() {
    ls -ld "$1" | awk '
    NR==1 {
        p=substr($1,2,9)
        if (length(p)!=9) {bad=1;exit}
        special=0; digits=""
        for (i=0;i<3;i++) {
            r=substr(p,1+3*i,1); w=substr(p,2+3*i,1); x=substr(p,3+3*i,1)
            if ((r!="r" && r!="-") || (w!="w" && w!="-") ||
                (x!="x" && x!="-" && x!="s" && x!="S" && x!="t" && x!="T")) {
                bad=1;exit
            }
            v=(r=="r" ? 4:0)+(w=="w" ? 2:0)+(x=="x" || x=="s" || x=="t" ? 1:0)
            digits=digits v
            if (i==0 && (x=="s" || x=="S")) special+=4
            if (i==1 && (x=="s" || x=="S")) special+=2
            if (i==2 && (x=="t" || x=="T")) special+=1
        }
        if (!bad) print special digits
    }
    END { if (NR!=1 || bad) exit 7 }'
}
# Backup pre-existing files OR their absent status; never overwrite first snapshot.
backup_all(){ [ ! -e "$BACKUP" ] || { say 'BACKUP_EXISTS: not overwriting immutable snapshot'; return 1; }; mkdir -p "$BACKUP" || return 1
for n in $NAMES; do
    f=`pathof "$n"`
    if [ -f "$f" ]; then
        MODE=`filemode "$f"` || { say "CANNOT_READ_ORIGINAL_MODE=$f"; return 1; }
        [ -n "$MODE" ] || return 1
        cp "$f" "$BACKUP/$n" || return 1
        echo "$MODE" > "$BACKUP/$n.mode" || return 1
    elif [ -e "$f" ]; then
        say "UNSUPPORTED_EXISTING_FILE_TYPE=$f"
        return 1
    else
        : > "$BACKUP/$n.absent" || return 1
    fi
done
: > "$BACKUP/complete" || return 1; sync || return 1; }
# Exact AltScreen proven deployment sequence: copy temporary, chmod, rename.
install_file() {
    SRC="$1"; DST="$2"; MODE="$3"
    TMP="$DST.renderer.new.$$"
    rm -f "$TMP" 2>/dev/null || return 1
    cp "$SRC" "$TMP" || { say "COPY_FAILED=$DST"; return 1; }
    chmod "$MODE" "$TMP" || { say "CHMOD_FAILED=$DST"; rm -f "$TMP"; return 1; }
    mv -f "$TMP" "$DST" || { say "RENAME_FAILED=$DST"; rm -f "$TMP"; return 1; }
    [ -f "$DST" ] || { say "MISSING_AFTER_RENAME=$DST"; return 1; }
}
restore_all(){ [ -f "$BACKUP/complete" ] || return 1
for n in $NAMES; do
    dest=`pathof "$n"`
    if [ -f "$BACKUP/$n.absent" ]; then
        rm -f "$dest" || return 1
    else
        [ -f "$BACKUP/$n" ] || return 1
        # New backups carry exact mode metadata. Never invent a replacement mode.
        [ -f "$BACKUP/$n.mode" ] || { say "RESTORE_MODE_MISSING=$n"; return 1; }
        MODE=`cat "$BACKUP/$n.mode"` || return 1
        case "$MODE" in
            [0-7][0-7][0-7][0-7]) ;;
            *) say "RESTORE_MODE_INVALID=$n"; return 1;;
        esac
        install_file "$BACKUP/$n" "$dest" "$MODE" || return 1
    fi
done
sync; }
TX=0
onexit(){ rc="$1"; trap - 0; if [ "$TX" -eq 1 ]; then say 'INSTALL_FAILED: restoring originals'; if restore_all; then rm -f "$PENDING" "$INSTALLED"; say 'AUTO_ROLLBACK_RESULT=PASS'; else say 'AUTO_ROLLBACK_RESULT=FAIL - do not reboot'; fi; fi; exit "$rc"; }
trap 'onexit $?' 0
preflight(){ [ -d "$APP/eso/hmi/lsd" ] || fail 'missing stock LSD directory'; [ -d "$SYSTEM/etc/eso/production" ] || fail 'missing system config directory'; [ -d "$APP/root" ] || fail 'missing stock root directory'; [ -f "$APP/eso/hmi/lsd/DSITracer.jar" ] || fail 'stock DSITracer.jar missing'; [ -f "$APP/eso/hmi/lsd/bundles_dsitracer_enabled.properties" ] || fail 'tracer bundle registry missing'; [ -f "$APP/eso/hmi/lsd/traceConfig.properties" ] || fail 'tracer config missing'; [ -f "$SYSTEM/etc/eso/production/smartphone_integrator.json" ] || fail 'smartphone integrator config missing'; }
install(){ say 'MODE=INSTALL'; checkrw "$SD" || fail 'SD not writable'; preflight
[ ! -e "$PENDING" ] || fail 'unfinished prior install: use ROLLBACK'; [ ! -e "$BACKUP" ] || fail 'existing backup retained: use ROLLBACK';
for n in libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar vc_render mib2high-lvds-control.sh mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf altscreen-dsitracer-capture vc_context_query; do [ -s "$PAY/$n" ] || fail "missing $n"; done
# Only check payload files exist; no on-car hashing, cksum, cmp or version gates.
# Keep unrelated configuration unchanged; use existing configuration as input.
awk -f "$MOD/source/bundles.awk" "$APP/eso/hmi/lsd/bundles_dsitracer_enabled.properties" > "$MOD/bundles.$$" || fail 'bundle patch failed'
awk -f "$MOD/source/trace.awk" "$APP/eso/hmi/lsd/traceConfig.properties" > "$MOD/trace.$$" || fail 'trace patch failed'
awk -f "$MOD/source/smartphone.awk" "$SYSTEM/etc/eso/production/smartphone_integrator.json" > "$MOD/smartphone.$$" || fail 'smartphone hook baseline unsupported'
checkrw "$APP" || fail 'app filesystem not writable'; checkrw "$SYSTEM" || fail 'system filesystem not writable'
mkdir -p "$APP/root/hooks" "$APP/eso/hmi/lsd/jars" || fail 'cannot create payload target directories'
backup_all || fail 'original backup failed'; : > "$PENDING" || fail 'cannot mark transaction'; TX=1
# Remove *only* retired AltScreen-owned graphics payloads after they are backed up.
# Restored bit-for-bit by restore_all on explicit rollback or failed install.
for obsolete in maneuver_gl flag_atlas.rgba; do
    obsolete_path=`pathof "$obsolete"`
    rm -f "$obsolete_path" || fail "cannot retire $obsolete"
    say "RETIRED_RENDERER=$obsolete"
done
for n in libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar vc_render mib2high-lvds-control.sh mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf altscreen-dsitracer-capture vc_context_query; do dest=`pathof "$n"`; say "INSTALLING=$n"; install_file "$PAY/$n" "$dest" `modeof "$n"` || fail "install failed $n"; done
install_file "$MOD/bundles.$$" "`pathof bundles_dsitracer_enabled.properties`" 644 || fail 'bundle install failed'
install_file "$MOD/trace.$$" "`pathof traceConfig.properties`" 644 || fail 'trace install failed'
install_file "$MOD/smartphone.$$" "`pathof smartphone_integrator.json`" 644 || fail 'smartphone install failed'
# Retire AltScreen's runtime mode selector; immutable backup preserves previous state.
rm -f "`pathof mib2high-altscreen-switch.conf`" || fail 'legacy switch cleanup failed' 
# Retire the old marker. It no longer controls AltScreen;
# backup_all already preserved its pre-install state for exact rollback.
rm -f "`pathof carplay_altscreen`" || fail 'legacy marker migration failed' 
: > "`pathof dsitracer_activated`" || fail 'tracer marker failed'
sync || fail 'pre-success sync failed'; : > "$INSTALLED" || fail 'cannot mark success'; sync || fail 'success sync failed'
rm -f "$PENDING" "$MOD/bundles.$$" "$MOD/trace.$$" "$MOD/smartphone.$$" || fail 'cleanup failed'; sync || fail 'final sync failed'
TX=0; say 'INSTALL_RESULT=PASS (deployment only; car behavior NOT TESTED)'; say 'NAVIGNORE=NOT_INCLUDED_INSTALL_SEPARATELY' ; say 'NAVIGNORE=UNTOUCHED'; say 'REBOOT_MMI_REQUIRED=YES; after reboot reconnect CarPlay and test; run script to collect'; say 'SAFE TO REMOVE SD'; }
rollback(){ say 'MODE=ROLLBACK'; checkrw "$SD" || fail 'SD not writable'; checkrw "$APP" || fail 'app not writable'; checkrw "$SYSTEM" || fail 'system not writable'; restore_all || fail 'restore failed: do not reboot'; rm -f "$INSTALLED" "$PENDING" "$ROLLBACK" || fail 'cannot clear flags'; : > "$MOD/rolledback.ok" || fail 'cannot mark rollback'; sync || fail 'rollback sync failed'; say 'ROLLBACK_RESULT=PASS'; say 'Reboot manually'; say 'SAFE TO REMOVE SD'; }
# AltScreen diagnostics collector: preserve per-process hook + stock iAP2 evidence.
# Collection health and feature behavior are separate: missing a navigation
# route or a CarPlay session is NOT a diagnostic failure.
collect(){
    say 'MODE=COLLECT'
    checkrw "$SD" || fail 'SD not writable'
    i=1
    while [ "$i" -le 99 ]; do
        if [ "$i" -lt 10 ]; then d="$COLLECT/run-0$i"; else d="$COLLECT/run-$i"; fi
        [ -e "$d" ] || break
        i=$((i+1))
    done
    [ "$i" -le 99 ] || fail 'collection folders full'
    mkdir -p "$d/live" "$d/logs" "$d/trace" || fail 'cannot create collection folder'
    RESULT=PASS
    COPIED=0
    echo "COLLECTION_EPOCH=`date 2>/dev/null || echo UNKNOWN`" > "$d/trace/collection-time.txt"
    MODE=on
    echo "ALTTRIP_COLLECT_MODE=ALWAYS_ON" > "$d/trace/summary.txt"
    echo "Expected: stock110+MAP111; RGI independent" >> "$d/trace/summary.txt"
    echo 'Session evaluation uses captured logs; absence of session activity is INCONCLUSIVE, not success.' >> "$d/trace/summary.txt"
    # AltScreen context-only diagnostics. Presence of a marker never proves pixels.
    SNAP="$d/trace/altscreen-context-snapshot.txt"
    echo 'EXPECTED_CONTEXT=76 DISPLAYABLES=58 ONLY' > "$SNAP"
    for m in /tmp/k2161-vc-context900-active /tmp/k2161-maneuver-gl-ready \
             /tmp/k2161-maneuver-geometry-ready /mnt/app/root/maneuver_gl \
             /mnt/app/root/hooks/flag_atlas.rgba; do
        if [ -e "$m" ]; then echo "$m UNEXPECTED_PRESENT" >> "$SNAP"; else echo "$m ABSENT" >> "$SNAP"; fi
    done
    if [ -x "$APP/root/vc_context_query" ]; then
        IPL_CONFIG_DIR=/etc/eso/production "$APP/root/vc_context_query" /eso/bin/apps/dmdt \
            > "$d/trace/altscreen-dmdt-current-context.txt" 2>&1 || true
    fi
    for n in $NAMES; do
        f=`pathof "$n"`
        if [ -f "$f" ]; then
            cp "$f" "$d/live/" || { RESULT=PARTIAL; echo "LIVE_COPY_FAILED=$f" >> "$d/trace/errors.txt"; }
        fi
    done
    # Optional root override enables deterministic host tests without modifying
    # the running K2161 log directory. The production default is /tmp.
    LOGROOT="${MIB_LOG_ROOT:-/tmp}"
    # Keep relevant current native, display, trace and independent Java logs.
    for f in "$LOGROOT"/carplay_hook.log "$LOGROOT"/carplay_hook.log.1 "$LOGROOT"/carplay_hook.log.2 \
             "$LOGROOT"/carplay_hook.log.3 "$LOGROOT"/carplay_hook.log.4 "$LOGROOT"/carplay_hook.log.5 \
             "$LOGROOT"/mib2high-carplay-rgi-native.log "$LOGROOT"/k2161-rgi-launcher.log \
             "$LOGROOT"/mib2high-lvds-control.log "$LOGROOT"/AltScreen-vc-context.log "$LOGROOT"/AltScreen-vc-context-transitions.log \
             "$LOGROOT"/AltScreen-vc-context.log.previous "$LOGROOT"/AltScreen-vc-context-transitions.log.previous \
             "$LOGROOT"/vc_render.log "$LOGROOT"/vc_render-launch.log "$LOGROOT"/mib2high-vc-render.profile \
             "$LOGROOT"/*rgi*.log "$LOGROOT"/*rgd*.log "$LOGROOT"/*bap*.log "$LOGROOT"/*DSI*.log "$LOGROOT"/*dsi*.log \
             "$LOGROOT"/*tracer*.log "$LOGROOT"/*zoom*.log "$LOGROOT"/*gauge*.log \
             "$LOGROOT"/AltScreen-dsitracer-*.log "$LOGROOT"/AltScreen-rgi-vc-java.log "$LOGROOT"/AltScreen-rgi-vc-java.log.1 \
             "$LOGROOT"/AltScreen-rgi-factory.log "$LOGROOT"/AltScreen-rgi-factory.log.1 \
             "$LOGROOT"/AltScreen-dsitracer-1798-*.log "$LOGROOT"/AltScreen-dsitracer-20991-*.raw \
             "$LOGROOT"/AltScreen-dsitracer-*-status.log "$LOGROOT"/AltScreen-dsitracer-launch-*.log; do
        [ -f "$f" ] || continue
        if cp "$f" "$d/logs/"; then
            echo "$f" >> "$d/trace/copied-files.txt"
            COPIED=$((COPIED+1))
        else
            RESULT=PARTIAL
            echo "LOG_COPY_FAILED=$f" >> "$d/trace/errors.txt"
        fi
    done
    # The native observer runs in dio_manager and writes to /dev/shmem,
    # which is not the same path as the separate Java RCC process mount.
    NATIVE_SHMEM="${MIB_NATIVE_SHMEM:-/dev/shmem}"
    for f in "$NATIVE_SHMEM"/mib2high-carplay-rgi-native.pid*.log \
             "$NATIVE_SHMEM"/mib2high-carplay-rgi-native.pid*.log.1 \
             "$NATIVE_SHMEM"/mib2high-carplay-rgi-native.pid*.log.2 \
             "$NATIVE_SHMEM/mib2high-carplay-rgi-native.log" \
             "$NATIVE_SHMEM/mib2high-carplay-rgi-native.log.1" \
             "$NATIVE_SHMEM/mib2high-carplay-rgi-native.log.2"; do
        [ -f "$f" ] || continue
        cp "$f" "$d/logs/native-${f##*/}" || { RESULT=PARTIAL; echo "NATIVE_SHMEM_COPY_FAILED=$f" >> "$d/trace/errors.txt"; }
    done
    # mm-ipod is launched with -l /dev/shmem/iap2.log. Preserve the complete
    # stock decoded 1D01 IdentificationInformation output, not a truncated line.
    IAP2_DIR="${MIB_IAP2_DIR:-/dev/shmem}"
    IAP2_FOUND=0
    for f in "$IAP2_DIR/iap2.log" "$IAP2_DIR/iap2.log.1" "$IAP2_DIR/iap2.log.2"; do
        [ -f "$f" ] || continue
        if cp "$f" "$d/logs/stock-${f##*/}"; then
            IAP2_FOUND=$((IAP2_FOUND+1))
            echo "STOCK_IAP2_LOG=$f" >> "$d/trace/copied-files.txt"
        else
            RESULT=PARTIAL
            echo "STOCK_IAP2_COPY_FAILED=$f" >> "$d/trace/errors.txt"
        fi
    done
    echo "STOCK_IAP2_LOG_FILES=$IAP2_FOUND" >> "$d/trace/summary.txt"
    : > "$d/trace/stock-iap2-1d01.txt"
    for f in "$d/logs/stock-iap2.log.2" "$d/logs/stock-iap2.log.1" "$d/logs/stock-iap2.log"; do
        [ -f "$f" ] || continue
        awk '
            /\[1D01\] IdentificationInformation:/ { in_ident=1 }
            in_ident { print }
            in_ident && /^\)/ { in_ident=0; print "--- END 1D01 ---" }
        ' "$f" >> "$d/trace/stock-iap2-1d01.txt" || RESULT=PARTIAL
    done
    if [ -s "$d/trace/stock-iap2-1d01.txt" ]; then
        echo '1D01_STOCK_CAPTURE=PRESENT' >> "$d/trace/summary.txt"
    else
        echo '1D01_STOCK_CAPTURE=NOT_FOUND' >> "$d/trace/summary.txt"
    fi
    # Separate outgoing sends from GPS trigger events for the next test.
    : > "$d/trace/rgi-5200-transmissions.txt"
    : > "$d/trace/gps-2700-triggers.txt"
    for f in "$d/logs"/native-mib2high-carplay-rgi-native.pid*.log*; do
        [ -f "$f" ] || continue
        grep '\[RGI5200_TX\]' "$f" >> "$d/trace/rgi-5200-transmissions.txt" 2>/dev/null || true
        grep '\[GPS2700\]' "$f" >> "$d/trace/gps-2700-triggers.txt" 2>/dev/null || true
    done
    RCC_SHMEM="${MIB_RCC_SHMEM:-/net/rcc/dev/shmem}"
    for f in "$RCC_SHMEM/mib2high-carplay-rgi.log" \
             "$RCC_SHMEM/mib2high-carplay-rgi.log.1" \
             "$RCC_SHMEM/mib2high-carplay-rgi.log.2" \
             "$RCC_SHMEM/mib2high-carplay-rgi.log.3" \
             "$RCC_SHMEM/AltScreen-rgi-factory.log" "$RCC_SHMEM/AltScreen-rgi-factory.log.1"; do
        [ -f "$f" ] || continue
        cp "$f" "$d/logs/" || { RESULT=PARTIAL; echo "JAVA_COPY_FAILED=$f" >> "$d/trace/errors.txt"; }
    done
    sloginfo > "$d/trace/sloginfo-raw.txt" 2>&1 || { RESULT=PARTIAL; echo SLOGINFO_FAILED >> "$d/trace/errors.txt"; }
    pidin ar > "$d/trace/pidin-ar.txt" 2>&1 || { RESULT=PARTIAL; echo PIDIN_FAILED >> "$d/trace/errors.txt"; }
    netstat -an > "$d/trace/netstat-an.txt" 2>&1 || { RESULT=PARTIAL; echo NETSTAT_FAILED >> "$d/trace/errors.txt"; }
    # Complete chronological native log order, including rotation. No old
    # historical regex that could hide newly added failure events.
    : > "$d/trace/AltScreen-native-timeline.txt"
    for suffix in .5 .4 .3 .2 .1 ''; do
        file="$d/logs/carplay_hook.log$suffix"
        [ -f "$file" ] || continue
        cat "$file" >> "$d/trace/AltScreen-native-timeline.txt" || RESULT=PARTIAL
    done
    grep -E 'ALTINPUT_MODE_EVENT|ALTSESSION_DIAG_FAIL|ALTSESSION_MAP|ALTSESSION_SETUP|ALTSESSION_SESSION|ALTSESSION_CLEANUP|ALTSESSION_VIDEO|ALTSESSION_ZOOM|ALTSESSION_CONTROL|ALTSESSION_TRACE|ALTSESSION_GAUGE|ALTSESSION_WHEEL|SETUP_|TEARDOWN|VC_CONTEXT' \
        "$d/trace/AltScreen-native-timeline.txt" > "$d/trace/AltScreen-key-events.txt" 2>/dev/null || true
    grep -E 'ALTSESSION_DIAG_FAIL|\[ERR\]|_FAILED| rc=-[1-9]|TIMEOUT|_REFUSED|_DROP' \
        "$d/trace/AltScreen-native-timeline.txt" > "$d/trace/AltScreen-failures.txt" 2>/dev/null || true
    # AltScreen: correlate input -> queue -> MAP command -> native ACK, without
    # conflating transport acknowledgments with visible zoom effects.
    grep -E 'ALTZOOM_ZOOM_|ALTSESSION_ZOOM_|ALTSESSION_WHEEL_DECISION' \
        "$d/trace/AltScreen-native-timeline.txt" > "$d/trace/AltScreen-zoom-trace.txt" 2>/dev/null || true
    : > "$d/trace/AltScreen-rgi-trigger-trace.txt"
    for f in "$d/logs"/native-mib2high-carplay-rgi-native.pid*.log*; do
        [ -f "$f" ] || continue
        grep '\[RGI5200_TRIGGER\]' "$f" >> "$d/trace/AltScreen-rgi-trigger-trace.txt" 2>/dev/null || true
    done
    # Rotated files can contain previous sessions. The latest boot marker is
    # never enough by itself: confirm the /tmp current-boot attestation and
    # live PID before reporting a verdict. No marker -> inconclusive, not PASS.
    PROOF="${MIB_MODE_PROOF:-/tmp/AltScreen-altscreen-current}"
    PROOF_PID=
    PROOF_MODE=
    if [ -f "$PROOF" ]; then
        PROOF_WORDS=`cat "$PROOF" 2>/dev/null` || PROOF_WORDS=
        for PROOF_WORD in $PROOF_WORDS; do
            case "$PROOF_WORD" in
                pid=*) PROOF_PID="${PROOF_WORD#pid=}" ;;
                mode=*) PROOF_MODE="${PROOF_WORD#mode=}" ;;
            esac
        done
    fi
    LIVE_PROCESS=NO
    case "$PROOF_PID" in
        ''|*[!0-9]*) ;;
        *) if kill -0 "$PROOF_PID" 2>/dev/null; then LIVE_PROCESS=YES; fi ;;
    esac
    echo "LIVE_MODE_PROOF_PID=${PROOF_PID:-MISSING}" >> "$d/trace/summary.txt"
    echo "LIVE_MODE_PROOF_MODE=${PROOF_MODE:-MISSING}" >> "$d/trace/summary.txt"
    echo "LIVE_MODE_PROOF_PROCESS=$LIVE_PROCESS" >> "$d/trace/summary.txt"
    # Only lines from the attested PID after its most recent mode event.
    # This excludes older sessions and other process IDs.
    if [ "$LIVE_PROCESS" = YES ] && [ "$PROOF_MODE" = "$MODE" ]; then
        awk -v wantpid="$PROOF_PID" -v wantmode="$PROOF_MODE" '
            {
                pid=""
                for(i=1;i<=NF;i++) if($i ~ /^pid=[0-9]+$/) pid=substr($i,5)
                if(pid != wantpid) next
                if(index($0,"ALTINPUT_MODE_EVENT")) {
                    seen=index($0,"selected=" toupper(wantmode)) ? 1 : 0
                    map=0; failures=0; events=0
                }
                if(seen) {
                    events++
                    if(index($0,"ALTSESSION_MAP_SETUP_READY")) map++
                    if(index($0,"ALTSESSION_DIAG_FAIL")) failures++
                }
            }
            END {print "MODE_BOOT_MATCH=" (seen ? "YES":"NO"); print "MAP_SETUP_AFTER_BOOT=" map+0; print "DIAG_FAIL_AFTER_BOOT=" failures+0; print "MODE_SESSION_EVENTS=" events+0}
        ' "$d/trace/AltScreen-native-timeline.txt" > "$d/trace/current-session.txt"
        cat "$d/trace/current-session.txt" >> "$d/trace/summary.txt"
        if grep 'MODE_BOOT_MATCH=YES' "$d/trace/current-session.txt" >/dev/null 2>&1; then
            if grep 'MAP_SETUP_AFTER_BOOT=0' "$d/trace/current-session.txt" >/dev/null 2>&1; then
                echo 'ALWAYS_ON_DIAGNOSTIC=MAP_SETUP_NOT_OBSERVED_IN_ATTESTED_PROCESS' >> "$d/trace/summary.txt"
            else
                echo 'ALWAYS_ON_DIAGNOSTIC=MAP_SETUP_OBSERVED_IN_ATTESTED_PROCESS' >> "$d/trace/summary.txt"
            fi
        else
            echo 'SESSION_DIAGNOSTIC=INCONCLUSIVE_NO_MATCHING_MODE_BOOT' >> "$d/trace/summary.txt"
        fi
    else
        echo 'SESSION_DIAGNOSTIC=INCONCLUSIVE_NO_LIVE_MODE_ATTESTATION' >> "$d/trace/summary.txt"
    fi
    echo "NATIVE_LOGS_COPIED=$COPIED" >> "$d/trace/summary.txt"
    echo 'LOG_FRESHNESS=CHECKED_AGAINST_LIVE_PROCESS_ATTESTATION_AND_LAST_BOOT_EVENT; when unavailable INCONCLUSIVE' >> "$d/trace/summary.txt"
    echo 'VC_CONTEXT_NOTE=requested != actual; inspect transition log and dmdt raw results' >> "$d/trace/summary.txt"
    echo 'ZOOM_NOTE=command ACK is not proof of visible zoom' >> "$d/trace/summary.txt"
    echo 'RGI_NOTE=ALTSTATS_RGI_QUEUE_LAG_MS and ALTSTATS_RGI_BAP_UPDATE_MS are Java shmem diagnostics' >> "$d/trace/summary.txt"
    echo 'SWIPE_NOTE=K2161Swipe SWIPE_INPUT_WRAPPER_INSTALLED and SWIPE_DSI_SEND_FAILED are Java shmem diagnostics' >> "$d/trace/summary.txt"
    echo 'RGI_NOTE=ALTSTATS_RGI_QUEUE_LAG_MS and ALTSTATS_RGI_BAP_UPDATE_MS are Java shmem diagnostics' >> "$d/trace/summary.txt"
    echo 'SWIPE_NOTE=K2161Swipe SWIPE_INPUT_WRAPPER_INSTALLED and SWIPE_DSI_SEND_FAILED are Java shmem diagnostics' >> "$d/trace/summary.txt"
    echo 'RGI_NOTE=ALTSTATS_RGI_QUEUE_LAG_MS and ALTSTATS_RGI_BAP_UPDATE_MS are Java shmem diagnostics' >> "$d/trace/summary.txt"
    echo 'SWIPE_NOTE=K2161Swipe SWIPE_INPUT_WRAPPER_INSTALLED and SWIPE_DSI_SEND_FAILED are Java shmem diagnostics' >> "$d/trace/summary.txt"
    echo 'RGI_NOTE=ALTSTATS_RGI_QUEUE_LAG_MS and ALTSTATS_RGI_BAP_UPDATE_MS are Java shmem diagnostics' >> "$d/trace/summary.txt"
    echo 'SWIPE_NOTE=K2161Swipe SWIPE_INPUT_WRAPPER_INSTALLED and SWIPE_DSI_SEND_FAILED are Java shmem diagnostics' >> "$d/trace/summary.txt"
    echo 'MAP_ONLY_BUILD=YES; CARD_AND_WAZE_RECORDER_NOT_COMPILED' >> "$d/trace/summary.txt"
    echo 'ALTHUD_RGI_NOTE=Search Java logs for ALTHUD_RGI_QUEUE_LAG_MS, ALTHUD_RGI_OUTPUT_SERVICE_MS and ALTHUD_HUD_SINK_MS' >> "$d/trace/summary.txt"
    echo 'ALTVC_CONTEXT_NOTE=Only displayable58 should be active; check dmdt context76 readback' >> "$d/trace/summary.txt"
    sync || fail 'collection sync failed'
    say "COLLECT_RESULT=$RESULT saved=$d mode=$MODE"
    say 'SAFE TO REMOVE SD'
}
[ -d "$SD" ] || fail 'SD not mounted'; [ -d "$MOD" ] || fail 'package missing'
if [ -f "$ROLLBACK" ]; then
    rollback
elif [ -f "$INSTALLED" ]; then
    collect
elif [ -f "$MOD/rolledback.ok" ]; then
    say 'ALREADY_ROLLED_BACK; no automatic reinstallation'
else
    install
fi
exit 0
