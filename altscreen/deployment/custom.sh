#!/bin/sh
set -u
PATH=/bin:/usr/bin:/sbin:/usr/sbin:$PATH; export PATH
SD="${MIB_SD:-/net/mmx/fs/sda0}"
APP="${MIB_APP:-/mnt/app}"
SYSTEM="${MIB_SYSTEM:-/mnt/system}"
PKGNAME=mib2high-carplay-rgi-v8s-simple
MOD="$SD/mod/$PKGNAME"; PAY="$MOD/payload"; BACKUP="$MOD/backup/pre-v8s"
INSTALLED="$MOD/installed.ok"; ROLLBACK="$MOD/ROLLBACK"
PENDING="$MOD/installing.flag"; COLLECT="$MOD/collected"
say(){ echo "[V8S] $*"; }
fail(){ say "ERROR $*"; exit 1; }
# Same write test / guarded mount -uw logic as the working v8g2 simple installer.
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
# All touched paths are indexed; no hidden earlier-install dependency.
NAMES='libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar vc_render mib2high-lvds-control.sh mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf bundles_dsitracer_enabled.properties traceConfig.properties smartphone_integrator.json carplay_altscreen dsitracer_activated v8l-dsitracer-capture'
pathof(){ case "$1" in
libmib2high-carplay-rgi.so) echo "$APP/root/hooks/$1";;
mib2high-carplay-rgi.jar) echo "$APP/eso/hmi/lsd/jars/$1";;
vc_render|v8l-dsitracer-capture|mib2high-lvds-control.sh|mib2high-lvds-profile.conf|mib2high-lvds-viewareas.conf) echo "$APP/root/$1";;
bundles_dsitracer_enabled.properties|traceConfig.properties|dsitracer_activated) echo "$APP/eso/hmi/lsd/$1";;
smartphone_integrator.json) echo "$SYSTEM/etc/eso/production/smartphone_integrator.json";;
carplay_altscreen) echo "$APP/carplay_altscreen";; esac; }
modeof(){ case "$1" in *.so|vc_render|v8l-dsitracer-capture|*.sh) echo 755;;*)echo 644;;esac; }
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
# Exact v8g2 proven deployment sequence: copy temporary, chmod, rename.
install_file() {
    SRC="$1"; DST="$2"; MODE="$3"
    TMP="$DST.v8s.new.$$"
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
for n in libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar vc_render mib2high-lvds-control.sh mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf v8l-dsitracer-capture; do [ -s "$PAY/$n" ] || fail "missing $n"; done
# Only check payload files exist; no on-car hashing, cksum, cmp or version gates.
# Keep unrelated configuration unchanged; use existing configuration as input.
awk -f "$MOD/source/bundles.awk" "$APP/eso/hmi/lsd/bundles_dsitracer_enabled.properties" > "$MOD/bundles.$$" || fail 'bundle patch failed'
awk -f "$MOD/source/trace.awk" "$APP/eso/hmi/lsd/traceConfig.properties" > "$MOD/trace.$$" || fail 'trace patch failed'
awk -f "$MOD/source/smartphone.awk" "$SYSTEM/etc/eso/production/smartphone_integrator.json" > "$MOD/smartphone.$$" || fail 'smartphone hook baseline unsupported'
checkrw "$APP" || fail 'app filesystem not writable'; checkrw "$SYSTEM" || fail 'system filesystem not writable'
mkdir -p "$APP/root/hooks" "$APP/eso/hmi/lsd/jars" || fail 'cannot create payload target directories'
backup_all || fail 'original backup failed'; : > "$PENDING" || fail 'cannot mark transaction'; TX=1
for n in libmib2high-carplay-rgi.so mib2high-carplay-rgi.jar vc_render mib2high-lvds-control.sh mib2high-lvds-profile.conf mib2high-lvds-viewareas.conf v8l-dsitracer-capture; do dest=`pathof "$n"`; say "INSTALLING=$n"; install_file "$PAY/$n" "$dest" `modeof "$n"` || fail "install failed $n"; done
install_file "$MOD/bundles.$$" "`pathof bundles_dsitracer_enabled.properties`" 644 || fail 'bundle install failed'
install_file "$MOD/trace.$$" "`pathof traceConfig.properties`" 644 || fail 'trace install failed'
install_file "$MOD/smartphone.$$" "`pathof smartphone_integrator.json`" 644 || fail 'smartphone install failed'
: > "`pathof carplay_altscreen`" || fail 'AltScreen marker failed'
: > "`pathof dsitracer_activated`" || fail 'tracer marker failed'
sync || fail 'pre-success sync failed'; : > "$INSTALLED" || fail 'cannot mark success'; sync || fail 'success sync failed'
rm -f "$PENDING" "$MOD/bundles.$$" "$MOD/trace.$$" "$MOD/smartphone.$$" || fail 'cleanup failed'; sync || fail 'final sync failed'
TX=0; say 'INSTALL_RESULT=PASS (deployment only; car behavior NOT TESTED)'; say 'NAVIGNORE=UNTOUCHED'; say 'Reboot MMI manually, test route/zoom/gauges, run custom script again to collect'; say 'SAFE TO REMOVE SD'; }
rollback(){ say 'MODE=ROLLBACK'; checkrw "$SD" || fail 'SD not writable'; checkrw "$APP" || fail 'app not writable'; checkrw "$SYSTEM" || fail 'system not writable'; restore_all || fail 'restore failed: do not reboot'; rm -f "$INSTALLED" "$PENDING" "$ROLLBACK" || fail 'cannot clear flags'; : > "$MOD/rolledback.ok" || fail 'cannot mark rollback'; sync || fail 'rollback sync failed'; say 'ROLLBACK_RESULT=PASS'; say 'Reboot manually'; say 'SAFE TO REMOVE SD'; }
# Collect files emitted by the already running car services. No additional services
# or network listeners are started, and no unverified QNX utilities are needed.
copyif(){
    [ -f "$1" ] || return 0
    cp "$1" "$2/" || return 1
}
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
    mkdir -p "$d/live" "$d/logs" "$d/trace" || fail 'cannot make collection folder'
    RESULT=PASS
    COPIED=0
    for n in $NAMES; do
        f=`pathof "$n"`
        if [ -f "$f" ]; then
            if cp "$f" "$d/live/"; then :; else RESULT=PARTIAL; echo "LIVE_COPY_FAILED=$f" >> "$d/trace/collection-errors.txt"; fi
        fi
    done
    # Explicit glob set from the known working v8g2 collector and v8i logs.
    for f in /tmp/carplay_hook.log /tmp/carplay_hook.log.1 /tmp/carplay_hook.log.2 /tmp/carplay_hook.log.3 \
             /tmp/mib2high-carplay-rgi-native.log /tmp/k2161-rgi-rx.log /tmp/k2161-rgi-launcher.log \
             /tmp/mib2high-lvds-control.log /tmp/vc_render.log /tmp/vc_render-launch.log \
             /tmp/mib2high-vc-render.profile /tmp/*rgi*.log /tmp/*rgd*.log /tmp/*bap*.log \
             /tmp/*DSI*.log /tmp/*dsi*.log /tmp/*tracer*.log /tmp/*zoom*.log \
             /tmp/*gauge*.log /tmp/v8l-dsitracer-*.log /tmp/v8l-dsitracer-status.log /tmp/v8l-dsitracer-launch.log /tmp/v8k-rgi-vc-java.log /tmp/v8k-rgi-vc-java.log.1 \
             /tmp/v8l-rgi-factory.log /tmp/v8l-rgi-factory.log.1 \
             /tmp/v8l-dsitracer-1798-*.log /tmp/v8l-dsitracer-20991-*.raw \
             /tmp/v8l-dsitracer-*-status.log /tmp/v8l-dsitracer-launch-*.log; do
        [ -f "$f" ] || continue
        if cp "$f" "$d/logs/"; then
            echo "$f" >> "$d/trace/copied-files.txt"
            COPIED=$((COPIED+1))
        else
            RESULT=PARTIAL
            echo "LOG_COPY_FAILED=$f" >> "$d/trace/collection-errors.txt"
        fi
    done
    # The established Java logger uses the RCC shared-memory path. Preserve
    # its raw output even if /tmp happens to be a different JVM namespace.
    if [ -f /net/rcc/dev/shmem/mib2high-carplay-rgi.log ]; then
        if cp /net/rcc/dev/shmem/mib2high-carplay-rgi.log "$d/logs/mib2high-carplay-rgi-java-shmem.log"; then
            echo '/net/rcc/dev/shmem/mib2high-carplay-rgi.log' >> "$d/trace/copied-files.txt"
            COPIED=$((COPIED+1))
        else
            RESULT=PARTIAL
            echo 'JAVA_SHMEM_LOG_COPY_FAILED' >> "$d/trace/collection-errors.txt"
        fi
    fi
    # v8m source-built factory logger writes to the same proven Java shared mount.
    # This resolves v8l's inaccessible /tmp Java-process namespace.
    RCC_SHMEM="${MIB_RCC_SHMEM:-/net/rcc/dev/shmem}"
    FACTORY_FILES=0
    for f in "$RCC_SHMEM/v8m-rgi-factory.log" "$RCC_SHMEM/v8m-rgi-factory.log.1"; do
        if [ -s "$f" ]; then
            if cp "$f" "$d/logs/"; then
                echo "$f" >> "$d/trace/copied-files.txt"
                COPIED=$((COPIED+1))
                FACTORY_FILES=$((FACTORY_FILES+1))
            else
                RESULT=PARTIAL
                echo "FACTORY_LOG_COPY_FAILED=$f" >> "$d/trace/collection-errors.txt"
            fi
        fi
    done
    # The main Java shmem log contains V8M_FACTORY records as fallback.
    if [ "$FACTORY_FILES" -eq 0 ]; then
        if [ -f "$d/logs/mib2high-carplay-rgi-java-shmem.log" ] && \
            grep 'V8M_FACTORY' "$d/logs/mib2high-carplay-rgi-java-shmem.log" >/dev/null 2>&1; then
            echo 'FACTORY_TRACE=MAIN_JAVA_LOG_FALLBACK' >> "$d/trace/copied-files.txt"
        else
            RESULT=PARTIAL
            echo 'FACTORY_TRACE_MISSING (verify guidance ran; inspect main Java log)' >> "$d/trace/collection-errors.txt"
        fi
    fi
    TRACE_FILES=0
    for f in "$d/logs"/v8l-dsitracer-1798-??.log "$d/logs"/v8l-dsitracer-20991-??.raw; do
        if [ -s "$f" ]; then TRACE_FILES=$((TRACE_FILES+1)); fi
    done
    if [ "$TRACE_FILES" -eq 0 ]; then
        RESULT=PARTIAL
        echo 'NO_BUFFERED_DSITRACER_DATA (capture may not have connected or no events emitted)' >> "$d/trace/collection-errors.txt"
    fi
    if [ "$COPIED" -eq 0 ]; then
        RESULT=PARTIAL
        echo 'NO_NATIVE_LOG_FILES_COPIED' >> "$d/trace/collection-errors.txt"
    fi
    if sloginfo > "$d/trace/sloginfo-raw.txt" 2>&1; then
        cp "$d/trace/sloginfo-raw.txt" "$d/sloginfo.txt" || RESULT=PARTIAL
    else
        RESULT=PARTIAL
        echo 'SLOGINFO_FAILED' >> "$d/trace/collection-errors.txt"
    fi
    pidin ar > "$d/pidin-ar.txt" 2>&1 || { RESULT=PARTIAL; echo 'PIDIN_FAILED' >> "$d/trace/collection-errors.txt"; }
    netstat -an > "$d/netstat-an.txt" 2>&1 || { RESULT=PARTIAL; echo 'NETSTAT_FAILED' >> "$d/trace/collection-errors.txt"; }
    # Focused index is optional; the original raw logs above are retained.
    grep -Ei 'V8I_|zoom|ZoomArea|ViewScreenViewPort|DSIMapViewer|DSIKOMO|DSIKombiSync|screenFormat|VC_TRACE|DSITracer|RGI|HUD|19810|gauge|dial' \
        "$d/trace/sloginfo-raw.txt" > "$d/trace/events.txt" 2>/dev/null || true
    {
        echo 'COLLECTION_VERSION=v8s'
        echo "LOG_FILES_COPIED=$COPIED"
        echo 'DSITRACER_STATUS=ports 1798+20991 recorder requested when AltScreen starts; check status and launch logs for execution'
        echo 'Gauge layout: LIVE DSIBAP service=50 FctID=54 bit0: 0=BIG, 1=SMALL; check V8N_GAUGE_BAP54 and VIEW_REACQUIRE logs'
        echo "FACTORY_LOG_FILES_COPIED=$FACTORY_FILES"
        echo 'DSIBAP=full DSICarKombi=full expected after install'
    } > "$d/trace/metadata.txt" || RESULT=PARTIAL
    sync || fail 'collection sync failed'
    say "COLLECT_RESULT=$RESULT saved=$d"
    say 'SAFE TO REMOVE SD'
}
[ -d "$SD" ] || fail 'SD not mounted'; [ -d "$MOD" ] || fail 'package missing'; if [ -f "$ROLLBACK" ]; then rollback; elif [ -f "$INSTALLED" ]; then collect; elif [ -f "$MOD/rolledback.ok" ]; then say 'ALREADY_ROLLED_BACK; no automatic reinstallation'; else install; fi
exit 0
