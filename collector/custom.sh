#!/bin/sh
# MIB2 High Software Collector v1.1.0-rc1
# SD-only research tool: never remount, patch or write to the MMI firmware.
# POSIX sh for QNX 6.5 / M.I.B.  Tested on simulated QNX command environments.
COLLECTOR_VERSION='1.1.0-rc1'
RULES_VERSION='4'
# Leave at least 512 MiB on the output SD when free space can be measured.
RESERVE_KIB=524288
MAX_CONTENT_SCAN=16777216

SD=''
OUT=''
LOG=''
CAPS=''
MANIFEST=''
ERRORS=''
WARNINGS=''
INVENTORY=''
CANDIDATES=''
SEEN=''
SRC_LIST=''
COPIES=0
FAILED=0
SKIPPED=0
LIMITATIONS=0
FAIL_STAGE='SD preflight'
IS_SYNC_OK=0
SYNC_FAILURE=0

screen() { echo "$*"; }
log() { [ -n "$LOG" ] && echo "$*" >> "$LOG" 2>/dev/null; return 0; }
msg() { screen "$*"; log "SCREEN | $*"; }
phase() { FAIL_STAGE="$2"; screen ''; msg "[$1/8] $2"; }
has() { command -v "$1" >/dev/null 2>&1; }
number() { case "$1" in ''|*[!0-9]*) return 1;; *) return 0;; esac; }
size_of() {
    _bytes=$(wc -c < "$1" 2>/dev/null) || { echo 'UNKNOWN'; return 1; }
    set -- $_bytes
    number "${1:-}" || { echo 'UNKNOWN'; return 1; }
    echo "$1"
}
# No unsafe SAFE TO REMOVE assertion if sync failed or unavailable.
flushing() { has sync && sync >/dev/null 2>&1; }
fail() {
    _why="$1"
    screen ''
    screen '========================================'
    screen ' COLLECTION STOPPED'
    screen '========================================'
    screen "Step: $FAIL_STAGE"
    screen "Reason: $_why"
    log "FATAL | stage=$FAIL_STAGE reason=$_why"
    if [ -n "$OUT" ] && [ -d "$OUT" ]; then
        # A failed final sync must NEVER leave a misleading COMPLETE marker.
        rm -f "$OUT/COLLECTION_COMPLETE.txt" >/dev/null 2>&1 || :
        echo "INCOMPLETE: $_why" > "$OUT/COLLECTION_INCOMPLETE.txt" 2>/dev/null
        if [ "$SYNC_FAILURE" -eq 1 ]; then
            flushing || :
            screen 'SD synchronization previously failed.'
            screen 'Do NOT remove SD during M.I.B. activity.'
        elif flushing; then
            screen 'SD synchronization finished.'
            screen 'It is SAFE TO REMOVE THE SD CARD.'
        else
            screen 'SD synchronization failed.'
            screen 'Do NOT remove SD during M.I.B. activity.'
        fi
    else
        screen 'No collection was started.'
        screen 'Return to the M.I.B. menu before removing SD.'
    fi
    screen 'No vehicle firmware was modified.'
    screen '========================================'
    exit 1
}
warn() { screen "  WARNING: $*"; log "WARNING | $*"; [ -n "$WARNINGS" ] && echo "$*" >> "$WARNINGS" 2>/dev/null; LIMITATIONS=$((LIMITATIONS + 1)); }
# A signal interruption is always marked as incomplete when SD is available.
trap 'fail "Collection interrupted by hangup signal."' 1
trap 'fail "Collection interrupted by user or launcher."' 2
trap 'fail "Collection interrupted by termination signal."' 15
# Counters are deliberately derived from persistent output files.
copy_error() { echo "$1|$2" >> "$ERRORS" 2>/dev/null; log "COPY_FAILED | $1 | $2"; }

# Restrict output and remount attempts to known removable mount prefixes.
removable() {
    case "$1" in
        /net/mmx/fs/sd*|/net/mmx/fs/usb*|/net/mmx/fs/mmc*|\
        /fs/sd*|/fs/usb*|/fs/mmc*|/media/sd*|/media/usb*|/mnt/sd*|/mnt/usb*) return 0;;
        *) return 1;;
    esac
}
# Each probe gets its own unique directory. No -r/-f recursive deletion.
probe_sd() {
    _probe_root="$1"
    _pd="$_probe_root/.mib2_collector_probe_$$"
    _p1="$_pd/one"; _p2="$_pd/two"
    _probe_ok=1
    mkdir "$_pd" >/dev/null 2>&1 || return 1
    echo "MIB2_TEST_$$" > "$_p1" 2>/dev/null || _probe_ok=0
    _read=''
    [ "$_probe_ok" -eq 1 ] && { IFS= read -r _read < "$_p1" 2>/dev/null || :; }
    [ "$_read" = "MIB2_TEST_$$" ] || _probe_ok=0
    [ "$_probe_ok" -eq 1 ] && mv "$_p1" "$_p2" >/dev/null 2>&1 || _probe_ok=0
    [ -f "$_p2" ] || _probe_ok=0
    rm -f "$_p1" "$_p2" >/dev/null 2>&1 || _probe_ok=0
    rmdir "$_pd" >/dev/null 2>&1 || _probe_ok=0
    [ "$_probe_ok" -eq 1 ] || return 1
    flushing || return 1
    return 0
}
try_sd() {
    _candidate="$1"
    removable "$_candidate" || return 1
    [ -d "$_candidate" ] || return 1
    if probe_sd "$_candidate"; then SD="$_candidate"; return 0; fi
    screen "  SD is not writable: $_candidate"
    if has mount; then
        screen '  Attempting guarded SD read-write remount...'
        # Only removable SD, never /mnt/app, /mnt/system, /etc, /ifs, etc.
        if mount -uw "$_candidate" >/dev/null 2>&1; then
            screen '  Remount command accepted. Rechecking SD...'
        else
            screen '  Remount command failed. Rechecking SD...'
        fi
        if probe_sd "$_candidate"; then SD="$_candidate"; return 0; fi
    else
        screen '  Cannot remount: mount command unavailable.'
    fi
    return 1
}

screen ''
screen '========================================'
screen " MIB2 SOFTWARE COLLECTOR $COLLECTOR_VERSION"
screen '========================================'
screen 'Read-only firmware research collection.'
screen 'MMI firmware will not be changed.'
phase 1 'Checking SD card and write access'
# Minimal commands required to perform the SD test itself.
for _must in mkdir rm rmdir mv sync wc; do
    has "$_must" || fail "Required SD-test command missing: $_must"
done
if [ -n "${MIB2_OUTPUT_ROOT:-}" ]; then
    screen '  Using explicitly selected SD path.'
    try_sd "$MIB2_OUTPUT_ROOT" || fail "Selected SD path is unavailable or not writable: $MIB2_OUTPUT_ROOT"
else
    # Prefer the same SD1 mounted by M.I.B., then other removable media.
    for _c in /net/mmx/fs/sda0 /fs/sda0 /net/mmx/fs/sdb0 /fs/sdb0 \
              /net/mmx/fs/sda1 /fs/sda1 /net/mmx/fs/sdc0 /fs/sdc0 \
              /net/mmx/fs/sdd0 /fs/sdd0 /fs/sd0 /fs/sd1 /fs/usb0 /fs/usb1 \
              /net/mmx/fs/usb0 /net/mmx/fs/usb1 /media/sd0 /mnt/sd0; do
        [ -d "$_c" ] || continue
        try_sd "$_c" && break
    done
    [ -n "$SD" ] || fail 'No writable removable SD/USB found, including after remount attempt.'
fi
screen "  SD write/read/rename/delete/sync: PASS"
screen "  SD path: $SD"

# Do not call unverified external date before command capability testing.
# The PID is unique for the run; repeated use on a new boot has a suffix guard.
_stamp="PID_$$"
_base="MIB2_SOFTWARE_COLLECTOR_${_stamp}"
OUT="$SD/$_base"
_i=1
while [ -d "$OUT" ] || [ -f "$OUT" ]; do
    _i=$((_i + 1)); OUT="$SD/${_base}_$_i"
done
mkdir "$OUT" >/dev/null 2>&1 || fail 'Cannot create collection folder on SD.'
LOG="$OUT/collector.log"; CAPS="$OUT/capabilities.txt"
MANIFEST="$OUT/manifest.tsv"; ERRORS="$OUT/copy_errors.tsv"
WARNINGS="$OUT/limitations.txt"; INVENTORY="$OUT/filesystem_inventory.txt"
CANDIDATES="$OUT/candidates.tsv"; SEEN="$OUT/selected_paths.txt"; SRC_LIST="$OUT/source_paths.txt"
mkdir "$OUT/files" "$OUT/system" "$OUT/discovery" >/dev/null 2>&1 || fail 'Cannot create required output subfolders.'
for _f in "$LOG" "$CAPS" "$ERRORS" "$WARNINGS" "$INVENTORY" "$CANDIDATES" "$SEEN" "$SRC_LIST"; do
    : > "$_f" || fail 'Could not initialize output files on SD.'
done
echo 'source|saved_relative_path|bytes|reason|checksum_method|checksum' > "$MANIFEST" || fail 'Cannot write manifest.'
log "COLLECTOR_BEGIN version=$COLLECTOR_VERSION SD=$SD"

phase 2 'Testing available tools on this MMI'
# Detect commands AND the actual options used by this script. Never assume GNU.
screen '  Testing each command (results also saved to SD):'
for _tool in sh echo date uname hostname mount df find grep awk sed tr strings \
    wc cp cat mkdir mv rm rmdir sync ls pidin use netstat sloginfo readlink \
    sha256sum md5sum md5 cksum tar gzip head sort uniq du file readelf objdump sleep; do
    if has "$_tool"; then
        echo "$_tool|AVAILABLE" >> "$CAPS"
        screen "  AVAILABLE: $_tool"
    else
        echo "$_tool|MISSING" >> "$CAPS"
        screen "  MISSING: $_tool"
    fi
done
_PROBE="$OUT/.tool_probe_$$"
echo 'CarPlay MHI2_TEST altscreen' > "$_PROBE" || fail 'Cannot create command test file.'
CAP_FIND=0; CAP_FIND_TYPE=0; CAP_AWK=0; CAP_GREP_I=0; CAP_GREP_E=0
CAP_STRINGS=0; CAP_TR=0; CAP_CP=0; CAP_CAT=0; CAP_DF=0; CAP_MKDIR_P=0; CAP_HASH='none'
if has find && find "$OUT" -name "${_PROBE##*/}" -print >/dev/null 2>&1; then CAP_FIND=1; fi
if [ "$CAP_FIND" -eq 1 ] && find "$OUT" -type f -print >/dev/null 2>&1; then CAP_FIND_TYPE=1; fi
if has awk && echo 'ABC' | awk '{if(tolower($0)=="abc") exit 0; exit 1}' >/dev/null 2>&1; then CAP_AWK=1; fi
if has grep && grep -i 'carplay' "$_PROBE" >/dev/null 2>&1; then CAP_GREP_I=1; fi
if [ "$CAP_GREP_I" -eq 1 ] && grep -i -E 'carplay|NoMatch' "$_PROBE" >/dev/null 2>&1; then CAP_GREP_E=1; fi
if has strings && strings "$_PROBE" >/dev/null 2>&1; then CAP_STRINGS=1; fi
if has tr && echo 'ABC' | tr 'ABC' 'abc' 2>/dev/null | grep 'abc' >/dev/null 2>&1; then CAP_TR=1; fi
if has cp && cp "$_PROBE" "$OUT/.cp_probe_$$" >/dev/null 2>&1 && [ -f "$OUT/.cp_probe_$$" ]; then CAP_CP=1; fi
if has cat && cat "$_PROBE" > "$OUT/.cat_probe_$$" 2>/dev/null && [ -f "$OUT/.cat_probe_$$" ]; then CAP_CAT=1; fi
if has df && df -k "$SD" >/dev/null 2>&1; then CAP_DF=1; fi
if mkdir -p "$OUT/.mkdir_option_probe_$$/child" >/dev/null 2>&1; then
    CAP_MKDIR_P=1
    rmdir "$OUT/.mkdir_option_probe_$$/child" "$OUT/.mkdir_option_probe_$$" >/dev/null 2>&1 || :
fi
for _tool in sha256sum md5sum cksum md5; do
    if has "$_tool" && "$_tool" "$_PROBE" >/dev/null 2>&1; then CAP_HASH="$_tool"; break; fi
done
rm -f "$_PROBE" "$OUT/.cp_probe_$$" "$OUT/.cat_probe_$$" >/dev/null 2>&1
for _row in \
    "find -name:$CAP_FIND" "find -type f:$CAP_FIND_TYPE" \
    "awk tolower:$CAP_AWK" "grep -i:$CAP_GREP_I" "grep -E:$CAP_GREP_E" \
    "strings:$CAP_STRINGS" "tr:$CAP_TR" "cp:$CAP_CP" "cat:$CAP_CAT" "df -k:$CAP_DF" "mkdir -p:$CAP_MKDIR_P"; do
    case "$_row" in *:1) screen "  OK: ${_row%:*}" ;; *:0) screen "  NO: ${_row%:*}" ;; esac
    echo "OPTION|${_row%:*}|${_row##*:}" >> "$CAPS"
done
screen "  Checksum tool: $CAP_HASH"
log "CHECKED_OPTIONS $_row hash=$CAP_HASH"
[ "$CAP_FIND" -eq 1 ] || fail 'find -name unavailable; cannot do safe dynamic file search.'
[ "$CAP_MKDIR_P" -eq 1 ] || fail 'mkdir -p unavailable; cannot preserve source directory structure.' 
[ "$CAP_CP" -eq 1 ] || [ "$CAP_CAT" -eq 1 ] || fail 'Neither cp nor cat can copy to SD.'
[ "$CAP_AWK" -eq 1 ] || warn 'awk unavailable: filename ranking and identity analysis reduced.'
[ "$CAP_GREP_I" -eq 1 ] || warn 'grep -i unavailable: content-based search disabled.'
[ "$CAP_STRINGS" -eq 1 ] || warn 'strings unavailable: binary-content grep is best-effort; names and paths are still scanned.'
[ "$CAP_HASH" != none ] || warn 'No checksum tool: all copies will be verified by byte count instead.'
[ "$CAP_DF" -eq 1 ] || warn 'df -k unavailable: collector cannot measure free SD space in advance.'

# Keep the exact collector source for reproducible research handoff.
if [ -r "$0" ] && [ -f "$0" ]; then
    if has cp && cp "$0" "$OUT/collector_source.sh" >/dev/null 2>&1; then
        log 'SOURCE_COPY_OK'
    elif has cat && cat "$0" > "$OUT/collector_source.sh" 2>/dev/null; then
        log 'SOURCE_COPY_OK fallback=cat'
    else
        log 'SOURCE_COPY_UNAVAILABLE'
    fi
fi

# Probe the specific diagnostics options before using them for collection.
DIAG_LIST=''
for _run in 'uname -a' 'hostname' 'pidin info' 'pidin ar' 'netstat -an' 'sloginfo'; do
    set -- $_run
    if has "$1" && "$@" >/dev/null 2>&1; then
        echo "DIAGNOSTIC|$_run|SUPPORTED" >> "$CAPS"
        screen "  OK diagnostic: $_run"
        DIAG_LIST="$DIAG_LIST;$_run"
    else
        echo "DIAGNOSTIC|$_run|UNAVAILABLE" >> "$CAPS"
        screen "  SKIP diagnostic: $_run"
    fi
done

# Capture QNX info from only the options that passed the runtime probes.
phase 3 'Reading MMI and firmware information'
for _run in 'uname -a' 'hostname' 'pidin info' 'pidin ar' 'netstat -an' 'sloginfo'; do
    case ";$DIAG_LIST;" in *";$_run;"*)
        set -- $_run
        log "INFO $_run"
        "$@" >> "$OUT/system/system_info.txt" 2>> "$LOG" || log "INFO_FAIL $_run"
        ;;
    esac
done
if has mount; then mount > "$OUT/system/mounts.txt" 2>> "$LOG" || log 'INFO_FAIL mount'; fi
if [ "$CAP_DF" -eq 1 ]; then df -k > "$OUT/system/df.txt" 2>> "$LOG" || log 'INFO_FAIL df'; fi
if [ -r /proc/version ] && has cat; then cat /proc/version >> "$OUT/system/system_info.txt" 2>> "$LOG"; fi
# Derive remaining SD space from the last numeric `Available` column.
# QNX df output is parsed defensively; unknown == no claimed space guarantee.
FREE_KIB='UNKNOWN'
REMAINING_KIB='UNKNOWN'
if [ "$CAP_DF" -eq 1 ] && [ "$CAP_AWK" -eq 1 ]; then
    df -k "$SD" > "$OUT/system/df_sd.txt" 2>> "$LOG"
    _avail=$(awk 'NR>1 {for(i=1;i<=NF;i++) if($i ~ /^[0-9]+$/) last=$i; if(NF>=4 && $4 ~ /^[0-9]+$/) available=$4} END{print available}' "$OUT/system/df_sd.txt" 2>/dev/null)
    if number "$_avail"; then FREE_KIB="$_avail"; REMAINING_KIB="$_avail"; fi
fi
screen "  SD free space (KiB): $FREE_KIB"
[ "$FREE_KIB" != UNKNOWN ] || warn 'SD free space could not be reliably parsed; copy failures will stop the run.'

# On your M.I.B. /net/mmx is the infotainment node, while /fs/sda0 is SD.
# Scan only firmware trees. Never recursively enumerate maps, user media or SD.
ROOTS=''
if [ -n "${MIB2_SCAN_ROOTS:-}" ]; then
    ROOTS="$MIB2_SCAN_ROOTS"
    log 'Using caller-supplied scan roots'
elif [ -d /net/mmx/mnt/app ] || [ -d /net/mmx/eso ]; then
    for _r in /net/mmx/mnt/app /net/mmx/mnt/system/etc /net/mmx/mnt/system/bin \
        /net/mmx/eso /net/mmx/ifs /net/mmx/etc /net/mmx/usr /net/mmx/lib \
        /net/mmx/bin /net/mmx/opt /net/mmx/mnt/eso; do
        [ -d "$_r" ] && ROOTS="$ROOTS $_r"
    done
else
    for _r in /mnt/app /mnt/system/etc /mnt/system/bin /eso /ifs /etc \
        /usr /lib /bin /opt /armle /mnt/eso; do
        [ -d "$_r" ] && ROOTS="$ROOTS $_r"
    done
fi
[ -n "$ROOTS" ] || fail 'No firmware directories could be found (neither QNX nor /net/mmx layout).'
# Explicitly avoid scanning any output SD root, even if overridden.
_SAFE_ROOTS=''
for _r in $ROOTS; do
    case "$_r" in "$SD"|"$SD"/*|/fs/sd*|/net/mmx/fs/sd*|/fs/usb*|/net/mmx/fs/usb*)
        warn "Excluded removable-media scan root: $_r";;
        *) _SAFE_ROOTS="$_SAFE_ROOTS $_r";;
    esac
done
ROOTS="$_SAFE_ROOTS"
[ -n "$ROOTS" ] || fail 'All scan roots refer to removable media, not firmware.'
log "SCAN_ROOTS=$ROOTS"

phase 4 'Searching the firmware filesystem'
screen '  Searching names and paths, not only a fixed list.'
for _r in $ROOTS; do
    [ -d "$_r" ] || continue
    screen "  Inspecting: $_r"
    log "INVENTORY_ROOT $_r"
    find "$_r" -print >> "$INVENTORY" 2>> "$LOG" || warn "Cannot fully enumerate directory: $_r"
done
[ -s "$INVENTORY" ] || fail 'Filesystem enumeration returned no paths.'
# This index is useful for porting even if not every file is copied.
if [ "$CAP_CP" -eq 1 ]; then
    cp "$INVENTORY" "$OUT/discovery/all_firmware_paths.txt" 2>> "$LOG" || log 'INDEX_COPY_FAIL'
else
    cat "$INVENTORY" > "$OUT/discovery/all_firmware_paths.txt" 2>> "$LOG" || log 'INDEX_COPY_FAIL'
fi

# Filename selection: broad dynamic search + exact baseline-critical names.
# No tr; uses confirmed awk for case folding on a single stream of pathnames.
if [ "$CAP_AWK" -eq 1 ]; then
    awk '
    {
      s=tolower($0); n=split(s,a,"/"); b=a[n]; why=""
      if (b ~ /\.(png|jpg|jpeg|gif|bmp|tga|ico|mp3|ogg|wav|mp4|mov|avi|mkv|ttf|otf|woff|swf)$/) next
      if (b ~ /^(lsd\.jxe|lsd\.sh|vc_render|mm-ipod|dio_manager|ipod-drvr-iap2\.so|libairplay\.so|dsitracer\.jar|carplay_altscreen|navactiveignore\.jar)$/) why="critical_core"
      else if (s ~ /(carplay|airplay|iap2|ipod|mirrorlink|android.?auto|smartphone|projection|headunit|infotainment|usblauncher)/) why="phone_projection"
      else if (s ~ /(routeguidance|guidance|navigation|navdata|maneuver|manoeuvre|navignore|\/nav[\/_.-]|navi[\/_.-])/) why="navigation"
      else if (s ~ /(bap|cluster|fpk|komo|kombi|instrument|headup|head_up|heads.up|hud|ddp2|displaycontext|viewarea|lvds|render|mapviewer)/) why="cluster_hud_display"
      else if (s ~ /(dsi|lsd|bundle|traceconfig|trace\.properties|keypanel|touchpad|touchscreen|rotary|joystick|inputdevice)/ || (s ~ /\/hmi\// && b ~ /\.(so|jar|jxe|cfg|conf|json|xml|properties|ini|lua|sh|bin)$/)) why="hmi_dsi_input"
      else if (b ~ /(\.cfg|\.conf|\.ini|\.json|\.xml|\.lua|\.properties|\.manifest|\.version|\.release)$/ && s ~ /(\/etc\/|\/production\/|\/config\/|\/hmi\/|\/mm\/)/) why="system_config"
      else if (b ~ /(swtrain|version|software|production|metainfo|manifest|buildinfo|firmware)/) why="firmware_identity"
      if(why!="") print $0 "|" why
    }' "$INVENTORY" > "$CANDIDATES" || fail 'awk filename selection failed.'
else
    # Reduced but valid POSIX-shell fallback; filenames are case-sensitive.
    while IFS= read -r _path; do
        case "$_path" in
            *CarPlay*|*carplay*|*airplay*|*iap2*|*ipod*|*navi*|*BAP*|*bap*|*DSI*|*dsi*|*fpk*|*cluster*|*lsd*|*render*|*KOMO*|*komo*|*smartphone*|*touchpad*|*guidance*|*vc_render*)
                echo "$_path|name_match" >> "$CANDIDATES";;
        esac
    done < "$INVENTORY"
fi
# Deduplicated and canonicalized at copy time.
screen '  Relevant filenames selected.'

# Copy helper. All outputs written only below $OUT/files. All source paths
# are absolute from QNX roots; never trust a malicious traversal component.
checksum() {
    case "$CAP_HASH" in
        sha256sum|md5sum|cksum) _h=$($CAP_HASH "$1" 2>/dev/null); set -- $_h; echo "${1:-UNKNOWN}";;
        md5) _h=$(md5 "$1" 2>/dev/null); set -- $_h; _last=''; for _v do _last="$_v"; done; echo "${_last:-UNKNOWN}";;
        *) echo 'SIZE_ONLY';;
    esac
}
copy_file() {
    _source="$1"; _reason="$2"
    [ -r "$_source" ] && [ -f "$_source" ] || { log "SKIP_UNREADABLE $_source"; return 0; }
    case "$_source" in
        /net/mmx/*) _canonical="${_source#/net/mmx}";;
        /*) _canonical="$_source";;
        *) log "SKIP_RELATIVE $_source"; return 0;;
    esac
    case "$_canonical" in *'/../'*|*'/./'*|*'//'*) copy_error "$_source" 'unsafe path'; return 1;; esac
    _rel="${_canonical#/}"
    _dest="$OUT/files/$_rel"
    [ -f "$_dest" ] && return 0
    _dir="${_dest%/*}"
    # mkdir -p must be tested before use. The script tests its option below.
    mkdir -p "$_dir" >/dev/null 2>&1 || { copy_error "$_source" 'mkdir -p failed'; return 1; }
    _srcbytes=$(size_of "$_source") || { copy_error "$_source" 'source byte count unavailable'; return 1; }
    # Preserve a reserve on the SD. If free-space parsing failed, rely on
    # strict copy/size/rename checks and report a limitation instead.
    _need_kib=$(((_srcbytes + 1023) / 1024))
    if [ "$REMAINING_KIB" != 'UNKNOWN' ]; then
        [ "$REMAINING_KIB" -ge $((RESERVE_KIB + _need_kib)) ] || {
            copy_error "$_source" 'SD reserve would be exhausted'; return 1;
        }
    fi
    if [ "$_srcbytes" -gt 16777216 ]; then
        screen "  Copying large file: ${_source##*/} ($_srcbytes bytes)"
    fi
    _temp="${_dest}.part.$$"
    if [ "$CAP_CP" -eq 1 ]; then
        cp "$_source" "$_temp" >/dev/null 2>&1 || { rm -f "$_temp"; copy_error "$_source" 'copy failed'; return 1; }
    else
        cat "$_source" > "$_temp" 2>/dev/null || { rm -f "$_temp"; copy_error "$_source" 'cat copy failed'; return 1; }
    fi
    _bytes=$(size_of "$_temp") || { rm -f "$_temp"; copy_error "$_source" 'destination byte count unavailable'; return 1; }
    if [ "$_bytes" != "$_srcbytes" ]; then
        rm -f "$_temp"; copy_error "$_source" "size mismatch ($_srcbytes vs $_bytes)"; return 1
    fi
    _hash=$(checksum "$_temp")
    if [ "$CAP_HASH" != none ]; then
        _sourcehash=$(checksum "$_source")
        [ "$_sourcehash" = "$_hash" ] || { rm -f "$_temp"; copy_error "$_source" 'checksum mismatch'; return 1; }
    fi
    mv "$_temp" "$_dest" >/dev/null 2>&1 || { rm -f "$_temp"; copy_error "$_source" 'rename failed'; return 1; }
    echo "$_source|$_rel|$_bytes|$_reason|$CAP_HASH|$_hash" >> "$MANIFEST" || { copy_error "$_source" 'manifest write failed'; return 1; }
    echo "$_source" >> "$SRC_LIST" || { copy_error "$_source" 'source index write failed'; return 1; }
    if [ "$REMAINING_KIB" != 'UNKNOWN' ]; then REMAINING_KIB=$((REMAINING_KIB - _need_kib)); fi
    COPIES=$((COPIES + 1))
    if [ $((COPIES % 25)) -eq 0 ]; then screen "  Copied $COPIES files (latest: ${_source##*/})"; fi
    log "COPIED $_source bytes=$_bytes reason=$_reason"
    return 0
}

# Prepare explicit representative firmware metadata; these files are searched
# by name above too. No dependency on them being present on other platforms.
phase 5 'Copying CarPlay, RGI, cluster and HMI files'
screen '  Copying only detected matching files.'
# Scan priority: selected files, then additional content discovery.
while IFS='|' read -r _src _why; do
    [ -n "$_src" ] || continue
    copy_file "$_src" "$_why"
done < "$CANDIDATES"

phase 6 'Finding extra files by their contents'
# Recognize binary data without relying on missing GNU grep -a / strings.
# Read-only grep -i on a bounded firmware file is tested at preflight.
if [ "$CAP_GREP_I" -eq 1 ] || [ "$CAP_STRINGS" -eq 1 ]; then
    if [ "$CAP_GREP_E" -eq 1 ]; then
        SEARCH_EXPR='carplay|airplay|iap2|routeguidance|maneuver|manoeuvre|head.?up|instrument.?cluster|mirrorlink|smartphone|bap|komo|fpk|dsicarplay|androidauto'
    fi
    # Candidate file types only. Avoid large user/media data or full images.
    _scanned=0
    while IFS= read -r _src; do
        [ -f "$_src" ] && [ -r "$_src" ] || continue
        case "$_src" in "$SD"/*|"$OUT"/*) continue;; esac
        # Only scan probable program/config files; still catches un-named .so.
        case "$_src" in *.so|*.so.*|*.dll|*.jxe|*.jar|*.json|*.cfg|*.conf|*.xml|*.properties|*.ini|*.lua|*.sh|*.txt|*.bin|*.so[0-9]|*/bin/*|*/sbin/*) :;; *) continue;; esac
        _bs=$(size_of "$_src") || continue
        [ "$_bs" -le "$MAX_CONTENT_SCAN" ] || continue
        # Already copied? Preserve consistent canon name for lookup.
        case "$_src" in /net/mmx/*) _relative="${_src#/net/mmx/}";; /*) _relative="${_src#/}";; *) continue;; esac
        [ -f "$OUT/files/$_relative" ] && continue
        _matched=0
        if [ "$CAP_STRINGS" -eq 1 ] && [ "$CAP_GREP_I" -eq 1 ]; then
            if [ "$CAP_GREP_E" -eq 1 ]; then
                strings "$_src" 2>/dev/null | grep -i -E "$SEARCH_EXPR" >/dev/null 2>&1 && _matched=1
            else
                strings "$_src" 2>/dev/null | grep -i 'carplay' >/dev/null 2>&1 && _matched=1
            fi
        elif [ "$CAP_GREP_I" -eq 1 ]; then
            if [ "$CAP_GREP_E" -eq 1 ]; then
                grep -i -E "$SEARCH_EXPR" "$_src" >/dev/null 2>&1 && _matched=1
            else
                grep -i 'carplay' "$_src" >/dev/null 2>&1 && _matched=1
            fi
        elif [ "$CAP_AWK" -eq 1 ]; then
            strings "$_src" 2>/dev/null | awk '{s=tolower($0);if(s ~ /(carplay|airplay|iap2|bap|routeguidance|kombiinstrument|fpk|maneuver)/)found=1} END{exit !found}' && _matched=1
        fi
        if [ "$_matched" -eq 1 ]; then
            echo "$_src|content_match" >> "$OUT/discovery/content_matches.tsv"
            copy_file "$_src" 'content_match'
        fi
        _scanned=$((_scanned + 1))
        if [ $((_scanned % 100)) -eq 0 ]; then
            screen "  Checked $_scanned additional software files..."
        fi
    done < "$INVENTORY"
    log "CONTENT_FILES_TESTED $_scanned"
else
    warn 'Content search skipped: no tested grep or strings method on this unit.'
fi

# Extract binary library names only if the firmware's strings command and awk
# have each passed runtime tests. Resolve them against the dynamic path index.
if [ "$CAP_STRINGS" -eq 1 ] && [ "$CAP_AWK" -eq 1 ]; then
    screen '  Resolving embedded shared-library dependencies...'
    _deps="$OUT/discovery/dependency_names.txt"
    : > "$_deps"
    while IFS= read -r _src; do
        [ -r "$_src" ] && [ -f "$_src" ] || continue
        _bs=$(size_of "$_src") || continue
        [ "$_bs" -le 67108864 ] || continue
        strings "$_src" 2>/dev/null | awk '{
            for(i=1;i<=NF;i++) {
                t=$i; gsub(/[^A-Za-z0-9_.+-]/,"",t)
                if (t ~ /^lib[A-Za-z0-9_.+-]*\.so([.][A-Za-z0-9_.+-]+)?$/) print t
            }
        }' >> "$_deps"
    done < "$SRC_LIST"
    awk '!seen[$0]++' "$_deps" > "$OUT/discovery/dependency_unique.txt"
    awk 'NR==FNR{deps[$0]=1;next} {n=split($0,p,"/");if(p[n] in deps) print $0}' \
        "$OUT/discovery/dependency_unique.txt" "$INVENTORY" > "$OUT/discovery/dependency_paths.txt"
    while IFS= read -r _src; do
        copy_file "$_src" 'shared_library_dependency'
    done < "$OUT/discovery/dependency_paths.txt"
else
    warn 'Library dependency extraction unavailable: no tested strings + awk combination.'
fi

# Identity is read from candidate metadata without relying on absent tr/date.
# Raw candidates are retained; heuristically selected identity never claims certainty.
_train='UNKNOWN'
if [ "$CAP_AWK" -eq 1 ]; then
    _ident="$OUT/discovery/software_identity_candidates.txt"
    : > "$_ident"
    while IFS= read -r _src; do
        case "$_src" in *version*|*Version*|*.properties|*.cfg|*.json|*.txt|*.xml|*.sh|*release*|*Release*) :;; *) continue;; esac
        _bs=$(size_of "$_src") || continue
        [ "$_bs" -le 1048576 ] || continue
        awk '{for(i=1;i<=NF;i++) {
            t=$i;gsub(/^[^A-Za-z0-9]+|[^A-Za-z0-9_.-]+$/,"",t)
            if(t ~ /^(MHI2|MHS2|MST2|MIB2)_/) print t
        }}' "$_src" 2>/dev/null >> "$_ident"
    done < "$SRC_LIST"
    IFS= read -r _first < "$_ident" 2>/dev/null || :
    [ -n "${_first:-}" ] && _train="$_first"
fi
{
    echo "Collector version: $COLLECTOR_VERSION"
    echo "Software train candidate: $_train"
    echo 'Source: discovery/software_identity_candidates.txt (when available)'
    echo "Scan roots: $ROOTS"
    if has uname; then echo "Environment: $(uname -a 2>/dev/null)"; fi
} > "$OUT/system/identity.txt"

phase 7 'Checking collection results'
# Manifest entries exclude heading. No false COMPLETE on copying failures.
_count='UNKNOWN'
if [ "$CAP_AWK" -eq 1 ]; then _count=$(awk 'END{print NR-1}' "$MANIFEST" 2>/dev/null); fi
if ! number "$_count"; then
    _count=$(wc -l < "$SRC_LIST" 2>/dev/null); set -- $_count; _count="${1:-0}"
fi
_failed=$(wc -l < "$ERRORS" 2>/dev/null); set -- $_failed; _failed="${1:-0}"
number "$_count" || fail 'Cannot count saved files.'
number "$_failed" || fail 'Cannot count file-copy errors.'
screen "  Saved firmware files: $_count"
screen "  Failed copies: $_failed"
screen "  Limitations/warnings: $LIMITATIONS"
if [ "$_count" -eq 0 ]; then fail 'No firmware files could be collected; see collector.log.'; fi
if [ "$_failed" -gt 0 ]; then
    IFS='|' read -r _bad_source _bad_reason < "$ERRORS" 2>/dev/null || :
    screen "  First failed file: ${_bad_source##*/}"
    screen "  Failure: ${_bad_reason:-unspecified}"
    fail "$_failed file copies failed. See copy_errors.tsv."
fi
# Every recorded destination must exist and still match the saved byte count.
_checked=0
while IFS='|' read -r _source _relative _expected _reason _method _hash; do
    [ "$_source" = source ] && continue
    [ -n "$_source" ] || continue
    _saved="$OUT/files/$_relative"
    [ -f "$_saved" ] || fail "Saved file disappeared: $_relative"
    _actual=$(size_of "$_saved") || fail "Cannot read saved file: $_relative"
    [ "$_actual" = "$_expected" ] || fail "Saved file length changed: $_relative"
    _checked=$((_checked + 1))
    if [ $((_checked % 100)) -eq 0 ]; then screen "  Verified $_checked saved files..."; fi
done < "$MANIFEST"
screen "  Final file-size checks: $_checked PASS"
[ "$_checked" -eq "$_count" ] || fail 'Saved manifest count does not match verified file count.'
{
    echo "Version: $COLLECTOR_VERSION"
    echo "Output SD: $SD"
    echo "Saved file count: $_count"
    echo "Failed copies: $_failed"
    echo "Runtime limitations: $LIMITATIONS"
    echo "Checksum mode: $CAP_HASH"
    echo "Discovery: dynamic filenames plus bounded content search"
    echo "Scan roots: $ROOTS"
    echo 'Firmware writes: none'
} > "$OUT/system/collection_summary.txt" || fail 'Could not write collection summary.'

phase 8 'Saving collection to SD'
if [ "$LIMITATIONS" -gt 0 ]; then
    _status='COMPLETE_WITH_LIMITATIONS'
else
    _status='COMPLETE'
fi
{
    echo "Status: $_status"
    echo "Collector version: $COLLECTOR_VERSION"
    echo "Collected firmware files: $_count"
    echo "Failed copies: $_failed"
    echo "Limitations: $LIMITATIONS"
    echo 'No vehicle firmware was modified.'
} > "$OUT/COLLECTION_COMPLETE.txt" || fail 'Could not write completion marker.'
log "COLLECTOR_END status=$_status files=$_count"
flushing || { SYNC_FAILURE=1; fail 'Final sync failed: SD contents cannot be confirmed saved.'; }
# Read back completion marker after sync; then do another sync so nothing remains.
_statusline=''
IFS= read -r _statusline < "$OUT/COLLECTION_COMPLETE.txt" 2>/dev/null || :
[ "$_statusline" = "Status: $_status" ] || fail 'Completion marker failed SD read-back.'
flushing || { SYNC_FAILURE=1; fail 'Second final sync failed.'; }
# No further writes to SD after this point.
screen ''
screen '========================================'
screen ' COLLECTION FINISHED'
screen '========================================'
screen "Firmware files saved: $_count"
screen "Copy errors: $_failed"
screen "Limitations: $LIMITATIONS"
screen "Status: $_status"
screen "Folder: ${OUT##*/}"
screen 'It is now SAFE TO REMOVE THE SD CARD.'
screen 'ZIP and share the complete folder.'
screen '========================================'
exit 0
