#!/bin/sh
# MIB2 High Software Collector v1.2 - firmware read-only, SD output only.
# External command set: mkdir, rm, mv, sync, cp, awk, find, wc, ls,
# df, mount, uname. This set is based on physical MIB2 High command tests;
# required options are also checked on the executing unit.
# No rmdir, recursive rm, strings, tr, date or checksum utilities.
VERSION='1.2'
RESERVE_KIB=524288
SD=''; OUT=''; LOG=''; CAP=''; ERR=''; WARN=''; MAN=''; PATHS=''
CORE=''; RELATED=''; FAIL_STAGE='SD verification'
COPIES=0; CORE_COPIES=0; RELATED_COPIES=0; LIMITATIONS=0
SYNC_FAILED=0; SIZE_MODE=''; LS_COLUMN=''; FREE_KIB='UNKNOWN'

screen() { echo "$*"; }
log() { [ -n "$LOG" ] && echo "$*" >> "$LOG" 2>/dev/null; return 0; }
info() { screen "$*"; log "MMI | $*"; }
stage() { FAIL_STAGE="$2"; screen ''; info "[$1/8] $2"; }
has() { command -v "$1" >/dev/null 2>&1; }
number() { case "$1" in ''|*[!0-9]*) return 1;; *) return 0;; esac; }
flush_sd() { sync >/dev/null 2>&1; }
fatal() {
    _why="$1"
    screen ''
    screen '========================================'
    screen ' COLLECTION STOPPED'
    screen "Stage: $FAIL_STAGE"
    screen "Reason: $_why"
    log "FATAL | $_why"
    if [ -n "$OUT" ] && [ -d "$OUT" ]; then
        rm -f "$OUT/COLLECTION_COMPLETE.txt" >/dev/null 2>&1 || :
        echo "INCOMPLETE: $_why" > "$OUT/COLLECTION_INCOMPLETE.txt" 2>/dev/null || :
        if [ "$SYNC_FAILED" -eq 0 ] && flush_sd; then
            screen 'SD synchronized; return to M.I.B. before removing SD.'
        else
            screen 'SD sync failed: do not remove during M.I.B. activity.'
        fi
    else
        screen 'No export started. Return to M.I.B. before removing SD.'
    fi
    screen 'MMI firmware has not been modified by this collector.'
    screen '========================================'
    exit 1
}
warn() {
    LIMITATIONS=$((LIMITATIONS + 1))
    info "  LIMITATION: $*"
    [ -n "$WARN" ] && echo "$*" >> "$WARN" 2>/dev/null || :
}
copy_fail() {
    echo "$1|$2" >> "$ERR" 2>/dev/null || :
    fatal "File copy failed: ${1##*/} ($2)"
}
trap 'fatal "Interrupted (SIGHUP)"' 1
trap 'fatal "Interrupted (SIGINT)"' 2
trap 'fatal "Interrupted (SIGTERM)"' 15

# Only specifically approved removable mount prefixes may receive writes/remount.
removable() {
    case "$1" in
        /net/mmx/fs/sd*|/net/mmx/fs/usb*|/net/mmx/fs/mmc*|\
        /fs/sd*|/fs/usb*|/fs/mmc*|/media/sd*|/media/usb*|/mnt/sd*|/mnt/usb*) return 0;;
        *) return 1;;
    esac
}
# Test a temporary *file*, not a directory; no rmdir dependency.
sd_probe() {
    _root="$1"; _a="$_root/.mib2_probe_$$"; _b="$_root/.mib2_probe_$$.renamed"
    [ ! -e "$_a" ] && [ ! -e "$_b" ] || return 1
    echo 'MIB2_COLLECTOR_SD_OK' > "$_a" 2>/dev/null || return 1
    _got=''
    IFS= read -r _got < "$_a" 2>/dev/null || :
    if [ "$_got" != 'MIB2_COLLECTOR_SD_OK' ]; then rm -f "$_a" >/dev/null 2>&1; return 1; fi
    if ! mv "$_a" "$_b" >/dev/null 2>&1; then rm -f "$_a" "$_b" >/dev/null 2>&1; return 1; fi
    if [ ! -f "$_b" ] || ! rm -f "$_b" >/dev/null 2>&1 || [ -e "$_b" ]; then return 1; fi
    flush_sd || return 1
    return 0
}
try_sd() {
    _candidate="$1"
    removable "$_candidate" && [ -d "$_candidate" ] || return 1
    if sd_probe "$_candidate"; then SD="$_candidate"; return 0; fi
    screen "  SD write test failed: $_candidate"
    if has mount; then
        screen '  Trying SD-only mount -uw, then repeating the write test...'
        mount -uw "$_candidate" >/dev/null 2>&1 || :
        if sd_probe "$_candidate"; then SD="$_candidate"; return 0; fi
    fi
    return 1
}

screen '========================================'
screen " MIB2 HIGH SOFTWARE COLLECTOR $VERSION"
screen '========================================'
screen 'Writes to removable SD only, never firmware.'
stage 1 'Verify SD writing, read-back, rename, delete and sync'
for _tool in mv rm sync mkdir; do
    has "$_tool" || fatal "Required SD command unavailable: $_tool"
done
if [ -n "${MIB2_OUTPUT_ROOT:-}" ]; then
    try_sd "$MIB2_OUTPUT_ROOT" || fatal "Selected SD inaccessible or not writable: $MIB2_OUTPUT_ROOT"
else
    for _candidate in /net/mmx/fs/sda0 /fs/sda0 /net/mmx/fs/sdb0 /fs/sdb0 \
        /net/mmx/fs/sda1 /fs/sda1 /net/mmx/fs/sdc0 /fs/sdc0 \
        /net/mmx/fs/sdd0 /fs/sdd0 /fs/sd0 /fs/sd1 /fs/usb0 /fs/usb1 \
        /net/mmx/fs/usb0 /net/mmx/fs/usb1 /media/sd0 /mnt/sd0; do
        [ -d "$_candidate" ] || continue
        try_sd "$_candidate" && break
    done
    [ -n "$SD" ] || fatal 'No removable SD passed the write/read/rename/delete/sync test.'
fi
screen "  SD PASS: $SD"

_base="MIB2_SOFTWARE_COLLECTOR_v1.2_PID_$$"; OUT="$SD/$_base"; _i=0
while [ -e "$OUT" ]; do _i=$((_i+1)); OUT="$SD/${_base}_$_i"; done
mkdir "$OUT" || fatal 'Cannot create SD output directory.'
LOG="$OUT/collector.log"; CAP="$OUT/capabilities.txt"; ERR="$OUT/copy_errors.tsv"
WARN="$OUT/limitations.txt"; MAN="$OUT/manifest.tsv"; PATHS="$OUT/discovery/all_firmware_paths.txt"
CORE="$OUT/discovery/core_candidates.tsv"; RELATED="$OUT/discovery/related_candidates.tsv"
mkdir "$OUT/discovery" "$OUT/system" "$OUT/files" || fatal 'Cannot create output folders.'
for _file in "$LOG" "$CAP" "$ERR" "$WARN" "$MAN" "$CORE" "$RELATED" "$PATHS"; do
    : > "$_file" || fatal 'Cannot initialize output file.'
done
echo 'source|saved_relative_path|bytes|reason|verification' > "$MAN" || fatal 'Cannot write manifest header.'
log "START | version=$VERSION SD=$SD"

stage 2 'Test every required command and option on SD'
# All external commands are from the previously vehicle-verified command set.
# Test required operations on this unit; missing mandatory capability = STOP.
for _tool in cp awk find ls; do
    if has "$_tool"; then info "  COMMAND OK: $_tool"; echo "$_tool|PRESENT" >> "$CAP"; \
    else echo "$_tool|MISSING" >> "$CAP"; fatal "Required collector command missing: $_tool"; fi
done
for _tool in wc df mount uname; do
    if has "$_tool"; then info "  COMMAND AVAILABLE: $_tool"; echo "$_tool|PRESENT" >> "$CAP"; \
    else info "  COMMAND UNAVAILABLE: $_tool"; echo "$_tool|MISSING" >> "$CAP"; fi
done
# Calibrate file length checks against TWO known lengths, not GNU ls layout.
_P1="$OUT/.size_probe_10"; _P2="$OUT/.size_probe_31"
echo '123456789' > "$_P1" || fatal 'Cannot write first size probe.'
echo '123456789012345678901234567890' > "$_P2" || fatal 'Cannot write second size probe.'
if has wc; then
    _n1=$(wc -c < "$_P1" 2>/dev/null) || _n1=''
    _n2=$(wc -c < "$_P2" 2>/dev/null) || _n2=''
    set -- $_n1; _n1="${1:-}"; set -- $_n2; _n2="${1:-}"
    if [ "$_n1" = 10 ] && [ "$_n2" = 31 ]; then SIZE_MODE='wc'; fi
fi
# Test ls -l on the SAME two files; infer the size column, never assume $5.
_L1="$OUT/.ls_probe_10"; _L2="$OUT/.ls_probe_31"
ls -l "$_P1" > "$_L1" 2>/dev/null || fatal 'ls -l failed during size calibration.'
ls -l "$_P2" > "$_L2" 2>/dev/null || fatal 'ls -l failed during second size calibration.'
LS_COLUMN=$(awk '
    NR==1 { for(i=2;i<=NF;i++) if($i=="10") candidate[i]=1; next }
    { for(i=2;i<=NF;i++) if(candidate[i] && $i=="31") { found=i; count++ } }
    END { if(count==1) print found }
' "$_L1" "$_L2" 2>/dev/null) || LS_COLUMN=''
if number "$LS_COLUMN"; then
    echo "ls -l calibrated size field: $LS_COLUMN" >> "$CAP"
    info "  ls -l byte-count field: $LS_COLUMN (measured)"
else
    [ "$SIZE_MODE" = wc ] || fatal 'Cannot calibrate ls -l file sizes and wc -c unavailable.'
    warn 'ls -l size-column calibration failed; verified wc -c will be used.'
fi
if [ "$SIZE_MODE" = wc ]; then
    info '  SIZE CHECK: wc -c (verified against 10 and 31 bytes)'
else
    SIZE_MODE='ls'; info '  SIZE CHECK: calibrated ls -l fallback (wc unavailable)'
fi
echo "SIZE_MODE|$SIZE_MODE" >> "$CAP"
# Explicitly verify the same find options used later, including -print.
if find "$OUT" -name '.size_probe_10' -print > "$OUT/.find_probe" 2>/dev/null && \
   [ -s "$OUT/.find_probe" ] && find "$OUT" -print > "$OUT/.find_all_probe" 2>/dev/null; then
    info '  find -name / -print: PASS'; echo 'find -name -print|PASS' >> "$CAP"
else
    fatal 'find -name / -print operation failed on SD.'
fi
# Test awk tolower, basename selection capability, and output redirection.
if echo 'KOMOVIEW.so' | awk '{n=split(tolower($0),x,"."); if(n==2 && x[1]=="komoview") print "PASS"}' > "$OUT/.awk_probe" && \
    { IFS= read -r _aw < "$OUT/.awk_probe" || :; [ "$_aw" = PASS ]; }; then
    info '  awk tolower / split: PASS'; echo 'awk tolower split|PASS' >> "$CAP"
else
    fatal 'awk filtering failed on this firmware.'
fi
if mkdir -p "$OUT/.mkdir_probe/child" && [ -d "$OUT/.mkdir_probe/child" ]; then
    info '  mkdir -p: PASS'; echo 'mkdir -p|PASS' >> "$CAP"
else
    fatal 'mkdir -p failed.'
fi
if cp "$_P1" "$OUT/.cp_probe" && [ -s "$OUT/.cp_probe" ]; then
    info '  cp: PASS'; echo 'cp|PASS' >> "$CAP"
else
    fatal 'cp file-copy test failed.'
fi
if has df && df -k "$SD" > "$OUT/system/df_sd.txt" 2>/dev/null; then
    info '  df -k: PASS (parsing verified separately)'; echo 'df -k|PASS' >> "$CAP"
else
    info '  df -k: UNAVAILABLE'; echo 'df -k|UNAVAILABLE' >> "$CAP"
fi
# File length subroutine uses ONLY runtime-tested tools/arguments.
size_of() {
    if [ "$SIZE_MODE" = wc ]; then
        _s=$(wc -c < "$1" 2>/dev/null) || return 1
        set -- $_s; _s="${1:-}"
    else
        _listing=$(ls -l "$1" 2>/dev/null) || return 1
        _s=$(echo "$_listing" | awk "{print \$$LS_COLUMN}" 2>/dev/null) || return 1
        set -- $_s; _s="${1:-}"
    fi
    number "$_s" || return 1
    echo "$_s"
}
[ "$(size_of "$_P1")" = 10 ] && [ "$(size_of "$_P2")" = 31 ] || fatal 'Selected size method failed read-back.'
rm -f "$_P1" "$_P2" "$_L1" "$_L2" "$OUT/.cp_probe" "$OUT/.awk_probe" \
    "$OUT/.find_probe" "$OUT/.find_all_probe" || fatal 'Probe file cleanup failed.'
[ ! -e "$_P1" ] && [ ! -e "$_P2" ] || fatal 'Probe files were not deleted.'
info '  ALL MANDATORY TESTS: PASS'

stage 3 'Save firmware and SD system information'
if has uname && uname -a > "$OUT/system/uname.txt" 2>/dev/null; then
    info '  uname -a: PASS'; echo 'uname -a|PASS' >> "$CAP"
else
    info '  uname -a: UNAVAILABLE'; echo 'uname -a|UNAVAILABLE' >> "$CAP"
fi
if has mount; then
    mount > "$OUT/system/mounts.txt" 2>/dev/null || warn 'mount listing unavailable.'
fi
# An unrecognized df layout is UNKNOWN, never an inferred free-space count.
# Accept a one-row table with an Available column, or a validated single-row
# headerless six-field layout: filesystem total used available use% mountpoint.
# Both MUST identify the selected removable volume, not merely any filesystem.
if [ -s "$OUT/system/df_sd.txt" ]; then
    _space_record=$(awk '
        NR==1 { for(i=1;i<=NF;i++) if(tolower($i) ~ /^avail/) col=i }
        NR>1 && col>0 && $col ~ /^[0-9]+$/ && $NF ~ /^\// {
            record=$col "|" $NF; rows++
        }
        NR==1 && col==0 && NF==6 &&
          $2 ~ /^[0-9]+$/ && $3 ~ /^[0-9]+$/ && $4 ~ /^[0-9]+$/ &&
          $5 ~ /^[0-9]+%$/ && $6 ~ /^\// && ($2-$3)==$4 {
            record=$4 "|" $6; rows++
        }
        END { if(rows==1) print record }
    ' "$OUT/system/df_sd.txt" 2>/dev/null) || _space_record=''
    case "$_space_record" in
        *'|'*)
            _space_value="${_space_record%%|*}"
            _reported_mount="${_space_record#*|}"
            _selected_mount="$SD"
            # The M.I.B. /net/mmx/ view is an alias for the target QNX mount.
            case "$_reported_mount" in /net/mmx/*) _reported_mount="${_reported_mount#/net/mmx}";; esac
            case "$_selected_mount" in /net/mmx/*) _selected_mount="${_selected_mount#/net/mmx}";; esac
            _reported_mount="${_reported_mount%/}"
            _selected_mount="${_selected_mount%/}"
            if number "$_space_value" && [ "$_reported_mount" = "$_selected_mount" ]; then
                FREE_KIB="$_space_value"
            fi
            ;;
    esac
fi
if [ "$FREE_KIB" = UNKNOWN ]; then
    warn 'df -k free-space format unrecognized; SD reserve cannot be guaranteed; copy errors remain fatal.'
else
    info "  SD free space: $FREE_KIB KiB (validated df -k layout)"
fi

stage 4 'Inventory firmware paths (single path-only pass)'
# Only firmware/application trees; NEVER scan SD, media, maps or personal data.
ROOTS=''
if [ -n "${MIB2_SCAN_ROOTS:-}" ]; then
    ROOTS="$MIB2_SCAN_ROOTS"
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
[ -n "$ROOTS" ] || fatal 'No firmware directories found.'
_any_root=0
for _r in $ROOTS; do
    [ -d "$_r" ] || continue
    case "$_r" in "$SD"|"$SD"/*|/fs/sd*|/net/mmx/fs/sd*|/fs/usb*|/net/mmx/fs/usb*|/mnt/sd*|/mnt/usb*|/media/sd*|/media/usb*)
        info "  EXCLUDING removable-media root: $_r"; continue;;
    esac
    _any_root=1
    info "  Reading filenames: $_r"
    find "$_r" -print >> "$PATHS" 2>> "$LOG" || warn "Cannot completely list firmware root: $_r"
done
[ "$_any_root" -eq 1 ] && [ -s "$PATHS" ] || fatal 'No firmware paths could be listed.'
# One AWK pass; select basename rather than all files inside broad directories.
# Do NOT read content of each discovered file; no recursive dependency dumping.
awk '
    {
      full=$0; lower=tolower(full); n=split(lower,a,"/"); b=a[n]
      if (index(full,"|")>0) next
      if (b=="" || b ~ /\.(png|jpg|jpeg|gif|bmp|tga|ico|mp3|ogg|wav|mp4|mov|avi|mkv|ttf|otf|woff|swf|db|sqlite|sqlite3|map|dat)$/) next
      why=""
      if (b ~ /^(lsd\.jxe|lsd\.sh|mm-ipod|dio_manager|smartphone_integrator|ipod-drvr-iap2\.so|libairplay\.so|libnvss_video\.so|libiap2client\.so(\.[0-9]+)*)$/ ||
          b ~ /^(iap2\.cfg|usblauncher_otg\.lua|smartphone_integrator\.json|dio_manager\.json|dsitracer\.jar|dsitracer-clientdata\.bin|traceconfig\.properties)$/ ||
          b ~ /^(bundles\.properties|bundles_dsitracer_enabled\.properties|bundles_dsitracer_disabled\.properties|bundles_headless\.properties)$/ ||
          b ~ /^(displaymanager|displaymanager\.json|dmdt|libdmdt_core\.so|librendersystem\.so|libluarendersystem\.so|vc_render)$/ ||
          b ~ /^(libdsicarplayproxy\.so|libdsikomoviewproxy\.so|libdsikomonavinfoproxy\.so|libdsikeypanelproxy\.so|libdsicarkombiproxy\.so)$/ ||
          b=="libasimmxdiagnavigationtypesproxy.so") why="core"
      else if (b ~ /^(libasimmxnaviconproxy|libasimmxroutedataproxy|libasimmxmulticlientrouteproviderproxy|libasimmxsmartphoneappproxy|libdsiasiainputproxy|libdsimapproxy|libasimmxdiagnavigationawtypesproxy)\.so$/)
           why="hmi_rgi_related"
      else if (b ~ /^libdsi(carplay|komo|kombi|carkombi|bap|keypanel|mapviewer|navigation|navinfopro|display|touch|input)[a-z0-9_]*proxy\.so(\.[0-9]+)*$/)
           why="dsi_proxy"
      else if (b ~ /^libasimmx[a-z0-9_]*(navigation|bap|combi|carplay)[a-z0-9_]*proxy\.so(\.[0-9]+)*$/)
           why="hmi_proxy"
      else if (b ~ /^lib[a-z0-9_+.-]*(carplay|airplay|iap2|bap|kombi|komo|fpk|cluster|headup|lvds|mapviewer|keypanel|nvss|videoovermost)[a-z0-9_+.-]*\.so(\.[0-9]+)*$/)
           why="related_library"
      else if (b ~ /^(videoovermost|devp-iso-mmx-mib2|airplayutil)(\..*)?$/) why="video_transport"
      else if (b ~ /^(clustercodinglvds|clustercodingmmi|displaymanagersplitscreen|enabledisplaymgrdebugmode|switchmokostorydisplaycontexttomainunitdisplay)\.sh$/)
           why="cluster_control_script"
      else if (b ~ /^(dsistartup|dsiadapter|dsiagentmapping|framework|servicemgr|hmi_startup|mm-renderer)\.(json|conf)(\.example)?$/)
           why="hmi_startup_config"
      else if (b=="version.txt" && lower ~ /\/hmi\/lsd\//) why="hmi_version"
      else if (b ~ /^(navactiveignore|dsitracer|[a-z0-9_.-]*(bap|komo|kombi|cluster|carplay|navigation))[a-z0-9_.-]*\.jar$/ && lower ~ /\/hmi\//)
           why="hmi_java"
      else if (b ~ /\.(cfg|conf|json|properties|lua|ini|xml)$/ && b ~ /(carplay|airplay|iap2|usblauncher|smartphone|dio_manager|displaymanager|lvds|viewarea|dsitracer|traceconfig|bundles|bap|komo|kombi|cluster|fpk|mapviewer|keypanel)/)
           why="related_config"
      if (why!="") print full "|" why
    }
' "$PATHS" > "$OUT/discovery/selected_candidates.tsv" || fatal 'Focused AWK filename filtering failed.'
while IFS='|' read -r _selected _type; do
    [ -n "$_selected" ] || continue
    if [ "$_type" = core ]; then echo "$_selected|$_type" >> "$CORE";
    else echo "$_selected|$_type" >> "$RELATED"; fi
done < "$OUT/discovery/selected_candidates.tsv"
[ -s "$CORE" ] || warn 'No exact core filenames found; check firmware path inventory.'
info '  Focused selection complete; no binary-content scanning.'

# Copy each selected read-only source to a .part file on SD, verify its
# byte count and rename. No fixed per-file size cap.
copy_selected() {
    _source="$1"; _why="$2"
    if [ -d "$_source" ]; then log "SKIP directory: $_source"; return 0; fi
    [ -f "$_source" ] || copy_fail "$_source" "selected file vanished or is not regular"
    [ -r "$_source" ] || copy_fail "$_source" "selected file unreadable"
    case "$_source" in
        /net/mmx/*) _canonical="${_source#/net/mmx}";;
        /*) _canonical="$_source";;
        *) log "SKIP non-absolute: $_source"; return 0;;
    esac
    case "$_canonical" in *'/../'*|*'/./'*|*'//'*) log "SKIP unsafe path: $_source"; return 0;; esac
    _rel="${_canonical#/}"
    case "$_rel" in *'|'*) log "SKIP unsupported name: $_source"; return 0;; esac
    _dest="$OUT/files/$_rel"
    [ ! -f "$_dest" ] || return 0
    mkdir -p "${_dest%/*}" || copy_fail "$_source" 'mkdir -p failed'
    _n=$(size_of "$_source") || copy_fail "$_source" 'cannot measure source bytes'
    _need=$(((_n + 1023) / 1024))
    if [ "$FREE_KIB" != UNKNOWN ]; then
        [ "$FREE_KIB" -ge $((RESERVE_KIB + _need)) ] || copy_fail "$_source" 'would exceed 512 MiB SD reserve'
    fi
    [ "$_n" -le 16777216 ] || info "  LARGE: ${_source##*/} ($_n bytes)"
    _part="${_dest}.part.$$"
    if ! cp "$_source" "$_part" >/dev/null 2>&1; then
        rm -f "$_part" >/dev/null 2>&1 || :
        copy_fail "$_source" 'cp failed'
    fi
    _verified=$(size_of "$_part") || { rm -f "$_part"; copy_fail "$_source" 'cannot measure saved bytes'; }
    [ "$_verified" = "$_n" ] || { rm -f "$_part"; copy_fail "$_source" 'byte count differs'; }
    mv "$_part" "$_dest" >/dev/null 2>&1 || { rm -f "$_part"; copy_fail "$_source" 'SD rename failed'; }
    echo "$_source|$_rel|$_verified|$_why|SIZE_ONLY" >> "$MAN" || copy_fail "$_source" 'manifest write failed'
    COPIES=$((COPIES+1))
    if [ "$_why" = core ]; then CORE_COPIES=$((CORE_COPIES+1)); \
    else RELATED_COPIES=$((RELATED_COPIES+1)); fi
    if [ "$FREE_KIB" != UNKNOWN ]; then FREE_KIB=$((FREE_KIB - _need)); fi
    if [ $((COPIES % 10)) -eq 0 ]; then info "  Saved $COPIES relevant files"; fi
    log "COPIED | $_source | $_verified bytes | $_why"
}

stage 5 'Copy exact CarPlay/RGI/AltScreen core filenames'
while IFS='|' read -r _path _why; do
    [ -n "$_path" ] && copy_selected "$_path" 'core'
done < "$CORE"
info "  Core files copied: $CORE_COPIES"

stage 6 'Copy narrowly related display, DSI and BAP files'
while IFS='|' read -r _path _why; do
    [ -n "$_path" ] && copy_selected "$_path" "$_why"
done < "$RELATED"
info "  Related files copied: $RELATED_COPIES"
info '  Full pathname inventory retained for follow-up requests.'

stage 7 'Verify every saved file and completion counts'
_verified=0
while IFS='|' read -r _src _rel _expected _why _mode; do
    [ "$_src" = source ] && continue
    [ -n "$_src" ] || continue
    _saved="$OUT/files/$_rel"
    [ -f "$_saved" ] || fatal "Saved file missing: $_rel"
    _actual=$(size_of "$_saved") || fatal "Cannot verify saved file: $_rel"
    [ "$_actual" = "$_expected" ] || fatal "Saved file size mismatch: $_rel"
    _verified=$((_verified+1))
done < "$MAN"
[ "$_verified" -gt 0 ] || fatal 'No matching firmware files were saved.'
[ "$_verified" -eq "$COPIES" ] || fatal 'Manifest/copied-file count mismatch.'
[ ! -s "$ERR" ] || fatal 'Copy errors recorded: export is incomplete.'
info "  All $_verified files passed byte-length verification."
warn 'Only file lengths were compared; no cryptographic checksum method is used.'
{
    echo "Collector version: $VERSION"
    echo "Source: read-only MIB2 firmware paths"
    echo "Core files: $CORE_COPIES"
    echo "Related files: $RELATED_COPIES"
    echo "Verified files: $_verified"
    echo "Verification: source and destination byte length (no hash)"
    echo "Runtime limitations: $LIMITATIONS"
    echo "SD free space remaining estimate (KiB): $FREE_KIB"
    echo "No files on the firmware were modified."
} > "$OUT/system/collection_summary.txt" || fatal 'Cannot write summary.'

stage 8 'Write result and synchronize SD'
if [ "$LIMITATIONS" -gt 0 ]; then _status='COMPLETE_WITH_LIMITATIONS'; \
else _status='COMPLETE'; fi
{
    echo "Status: $_status"
    echo "Collector version: $VERSION"
    echo "Verified files: $_verified"
    echo "Core files: $CORE_COPIES"
    echo "Related files: $RELATED_COPIES"
    echo "Limitations: $LIMITATIONS"
    echo 'Vehicle firmware writes: none'
} > "$OUT/COLLECTION_COMPLETE.txt" || fatal 'Cannot write completion marker.'
flush_sd || { SYNC_FAILED=1; fatal 'First final SD sync failed'; }
_statusline=''
IFS= read -r _statusline < "$OUT/COLLECTION_COMPLETE.txt" 2>/dev/null || :
[ "$_statusline" = "Status: $_status" ] || fatal 'Completion marker read-back failed.'
flush_sd || { SYNC_FAILED=1; fatal 'Second final SD sync failed'; }
# Never write to SD after the last successful sync.
screen ''
screen '========================================'
screen ' COLLECTION FINISHED'
screen "Firmware files: $_verified"
screen "Core: $CORE_COPIES  Related: $RELATED_COPIES"
screen "Limitations: $LIMITATIONS"
screen "Status: $_status"
screen "Folder: ${OUT##*/}"
screen 'It is now SAFE TO REMOVE THE SD CARD.'
screen 'Review files for personal data before sharing.'
screen '========================================'
exit 0
