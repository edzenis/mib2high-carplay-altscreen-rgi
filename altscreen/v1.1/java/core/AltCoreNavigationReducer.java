package com.luka.carplay.routeguidance;
/** Pure CarPlay route-state reducer. Platform input maps transport keys to Fields.
 * No BAP, QNX, OSGi, Audi, input-service, or output dependency. */
public final class AltCoreNavigationReducer {
    public static interface Fields {
        boolean has(String key);
        int num(String key, int fallback);
        String str(String key);
        String str(String key, String fallback);
        int size();
        int[] intList(String key);
        long num64(String key, long fallback);
    }
    public static interface Logger { void debug(String text); }
    private static final int MAX_MANEUVERS = 32;
    private static final int ROUTE_STATE_NO_ROUTE_SET=0;
    private static final int ROUTE_STATE_ROUTE_SET=1;
    private static final int ROUTE_STATE_REROUTING=5;
    private static final int ROUTE_STATE_ACCEPT_ALL_MANEUVER_IDX=ROUTE_STATE_REROUTING;
    private final AltCoreRouteState state;
    private final Logger logger;
    private boolean hasRouteUpdate;
    public AltCoreNavigationReducer(AltCoreRouteState state, Logger logger) {
        if (state == null || logger == null) throw new IllegalArgumentException("state/logger");
        this.state=state; this.logger=logger;
    }
    public void reset() { hasRouteUpdate=false; state.reset(); }
    public void parse(Fields d) {
        state.clearDirty();

        boolean isRouteUpdateDelta =
            d.has("route_state") ||
            d.has("maneuver_state") ||
            d.has("maneuver_count") ||
            d.has("maneuver_list") ||
            d.has("dist_dest_m") ||
            d.has("dist_maneuver_m") ||
            d.has("eta_seconds") ||
            d.has("time_remaining_seconds") ||
            d.has("current_road") ||
            d.has("destination") ||
            d.has("visible_in_app") ||
            d.has("component_ids") ||
            d.has("component_count") ||
            d.has("source_supports_rg");
        if (isRouteUpdateDelta) {
            hasRouteUpdate = true;
        }

        if (d.has("disconnect_reason")) {
            String v = d.str("disconnect_reason");
            if (!strEq(state.disconnectReason, v)) {
                state.disconnectReason = v;
                state.markDirty(AltCoreRouteState.DIRTY_DISCONNECT);
            }
        } else if (state.disconnectReason != null && d.size() > 0) {
            /* One-shot field: clear on any subsequent non-empty update */
            logger.debug( "Clearing disconnect_reason on new bus update");
            state.disconnectReason = null;
            state.markDirty(AltCoreRouteState.DIRTY_DISCONNECT);
        }
        if (d.has("source_supports_rg")) {
            int v = d.num("source_supports_rg", -1);
            if (v != state.sourceSupportsRg) {
                state.sourceSupportsRg = v;
                state.markDirty(AltCoreRouteState.DIRTY_SOURCE_SUPPORTS_RG);
                /*
                 * The reference route-guidance processor uses the "source supports route guidance" signal to suppress
                 * route guidance updates. For the HUD bridge, treat support==0 as a hard clear
                 * to prevent stale icons/distances from lingering.
                 */
                if (v == 0) {
                    if (state.routeState != ROUTE_STATE_NO_ROUTE_SET) {
                        state.routeState = ROUTE_STATE_NO_ROUTE_SET;
                        state.markDirty(AltCoreRouteState.DIRTY_ROUTE_STATE);
                    }
                    if (state.maneuverCount != 0) {
                        state.maneuverCount = 0;
                        state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_COUNT);
                    }
                    state.maneuverOrder = null;
                    state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_LIST);
                    state.clearAllManeuverSlots();
                    state.laneGuidanceShowing = -1;
                    state.laneGuidanceTotal = -1;
                    state.laneGuidanceIndex = -1;
                    state.laneGuidanceSlot = -1;
                    state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_ICON | AltCoreRouteState.DIRTY_MANEUVER_TEXT | AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
        }
        if (d.has("route_state")) {
            int v = d.num("route_state", -1);
            /*
             * route_state=0 handling.
             *
             * iOS sends transient route_state=0 during route setup and periodically
             * during active navigation (~every 30s).  A full per-slot clear here
             * causes permanent data loss because iOS doesn't resend 0x5202 for
             * already-sent maneuver indices, and it causes visible flicker on the
             * cluster (NO_SYMBOL + distance=-1 for ~1s each cycle).
             *
             * Perform a full per-slot clear only when there is positive evidence of a
             * genuine route end: sourceSupportsRg=0 or disconnect_reason set.
             * Transient route_state=0 only resets count and order so BAPBridge
             * can suppress maneuver output without losing cached data.
             */
            if (v == 0) {
                if (state.routeState != 0) {
                    state.routeState = 0;
                    state.markDirty(AltCoreRouteState.DIRTY_ROUTE_STATE);
                }
                if (state.maneuverCount != 0) {
                    state.maneuverCount = 0;
                    state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_COUNT);
                }
                state.maneuverOrder = null;
                state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_LIST);
                state.laneGuidanceShowing = -1;
                state.laneGuidanceTotal = -1;
                state.laneGuidanceIndex = -1;
                state.laneGuidanceSlot = -1;
                state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_ICON | AltCoreRouteState.DIRTY_MANEUVER_TEXT | AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                /* Per-slot data (mType, mTurnAngle, etc.) is NOT cleared here.
                 * It is preserved so that when count>0 returns, BAPBridge can
                 * immediately display the correct maneuver icons.
                 * Full per-slot clear only happens on sourceSupportsRg=0 or
                 * disconnect_reason (handled elsewhere). */
            } else if (v > 0) {
                if (v != state.routeState) {
                    state.routeState = v;
                    state.markDirty(AltCoreRouteState.DIRTY_ROUTE_STATE);
                }
            }
        }
        if (d.has("maneuver_state")) {
            int v = d.num("maneuver_state", -1);
            /* Reference EManeuverState: 0..3 are valid (CONTINUE/INITIAL/PREPARE/EXECUTE). */
            if (v >= 0 && v <= 3 && v != state.maneuverState) {
                state.maneuverState = v;
                state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_STATE);
            }
        }
        if (d.has("maneuver_count")) {
            int v = d.num("maneuver_count", 0);
            if (v != state.maneuverCount) {
                state.maneuverCount = v;
                state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_COUNT);
                /*
                 * Explicit clear: iOS (and injected test frames) can send maneuver_count=0 without
                 * sending maneuver_list. Treat that as a real clear and drop cached order,
                 * otherwise BAPBridge may keep re-sending stale maneuver icons.
                 */
                if (v == 0) {
                    state.maneuverOrder = null;
                    state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_LIST);
                    /*
                     * Don't clear per-slot data (mType, mTurnAngle, etc.) here.
                     *
                     * The slot data was written by the C hook from real 0x5202 ManeuverUpdate
                     * messages and corresponds to valid maneuver info.  Bus deltas only report
                     * changes, so clearing Java state would prevent the bus from re-sending unchanged values,
                     * losing the data until a new 0x5202 arrives.
                     *
                     * maneuver_count=0 can be transient - iOS briefly clears the count before
                     * re-populating (observed at route start). Preserving slot data avoids
                     * the race where maneuver_list advances to slots whose data was cleared.
                     *
                     * Full per-slot clear still happens on:
                     *   - route_state=0  (genuine route end / reset)
                     *   - source_supports_rg=0  (hard reset)
                     */
                }
            }
        }
        if (d.has("maneuver_list")) {
            /*
             * Reference behavior:
             * - maneuver_list missing -> keep previous (null or last value)
             * - maneuver_list present but empty -> treat as an explicit empty list
             */
            String raw = d.str("maneuver_list", null);
            int[] v;
            if (raw == null) {
                v = null;
            } else if (raw.length() == 0) {
                v = new int[0];
            } else {
                v = d.intList("maneuver_list");
            }
            if (!intArrayEq(state.maneuverOrder, v)) {
                state.maneuverOrder = v;
                state.markDirty(AltCoreRouteState.DIRTY_MANEUVER_LIST);
            }
        }
        if (d.has("visible_in_app")) {
            int raw = d.num("visible_in_app", -1);
            int v = (raw == 0 || raw == 1) ? raw : -1;
            if (v != state.visibleInApp) {
                state.visibleInApp = v;
                state.markDirty(AltCoreRouteState.DIRTY_VISIBLE_IN_APP);
            }
        }
        if (d.has("lane_guidance_showing")) {
            int v = d.num("lane_guidance_showing", -1);
            if (v != state.laneGuidanceShowing) {
                state.laneGuidanceShowing = v;
                state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
            }
        }
        if (d.has("lane_guidance_total")) {
            int v = d.num("lane_guidance_total", -1);
            if (v != state.laneGuidanceTotal) {
                state.laneGuidanceTotal = v;
                state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
            }
        }
        if (d.has("lane_guidance_index")) {
            int v = d.num("lane_guidance_index", -1);
            if (v != state.laneGuidanceIndex) {
                state.laneGuidanceIndex = v;
                state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
            }
        }
        if (d.has("lane_guidance_slot")) {
            int v = d.num("lane_guidance_slot", -1);
            if (v != state.laneGuidanceSlot) {
                state.laneGuidanceSlot = v;
                state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
            }
        }

        if (d.has("dist_dest_m")) {
            int v = d.num("dist_dest_m", -1);
            if (v != state.distDestM) {
                state.distDestM = v;
                state.markDirty(AltCoreRouteState.DIRTY_DIST_DEST);
            }
        }
        if (d.has("dist_maneuver_m")) {
            int v = d.num("dist_maneuver_m", -1);
            if (v != state.distManeuverM) {
                state.distManeuverM = v;
                state.markDirty(AltCoreRouteState.DIRTY_DIST_MAN);
            }
        }
        if (d.has("eta_seconds")) {
            int v = d.num("eta_seconds", -1);
            if (v != state.etaSeconds) {
                state.etaSeconds = v;
                state.markDirty(AltCoreRouteState.DIRTY_ETA);
            }
        }
        if (d.has("time_remaining_seconds")) {
            long v = d.num64("time_remaining_seconds", -1);
            if (v != state.timeRemainingSeconds) {
                state.timeRemainingSeconds = v;
                state.markDirty(AltCoreRouteState.DIRTY_TIME_REMAINING);
            }
        }
        if (d.has("current_road")) {
            String v = d.str("current_road");
            if (!strEq(state.currentRoad, v)) {
                state.currentRoad = v;
                state.markDirty(AltCoreRouteState.DIRTY_CURRENT_ROAD);
            }
        }
        if (d.has("destination")) {
            String v = d.str("destination");
            if (!strEq(state.destination, v)) {
                state.destination = v;
                state.markDirty(AltCoreRouteState.DIRTY_DESTINATION);
            }
        }

        /* Parse lane-guidance cache lg0_, lg1_, ... */
        for (int i = 0; i < MAX_MANEUVERS; i++) {
            String p = "lg" + i + "_";

            if (d.has(p + "index")) {
                int v = d.num(p + "index", -1);
                if (v != state.lgIndex[i]) {
                    state.lgIndex[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_count")) {
                int v = d.num(p + "lane_count", -1);
                if (v != state.lgLaneCount[i]) {
                    state.lgLaneCount[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_positions")) {
                int[] v = d.intList(p + "lane_positions");
                if (!intArrayEq(state.lgLanePositions[i], v)) {
                    state.lgLanePositions[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_directions")) {
                int[] v = d.intList(p + "lane_directions");
                if (!intArrayEq(state.lgLaneDirections[i], v)) {
                    state.lgLaneDirections[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_status")) {
                int[] v = d.intList(p + "lane_status");
                if (!intArrayEq(state.lgLaneStatus[i], v)) {
                    state.lgLaneStatus[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_angles")) {
                int[][] v = parseLaneAnglesMatrix(d.str(p + "lane_angles", null));
                if (!intMatrixEq(state.lgLaneAngles[i], v)) {
                    state.lgLaneAngles[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
        }

        /* Parse maneuvers m0_, m1_, ... */
        for (int i = 0; i < MAX_MANEUVERS; i++) {
            String p = "m" + i + "_";

            /* Route-order gating: discard maneuver updates until a route update exists or when the index is too old. */
            /*
             * Gate the whole maneuver slot, not a hand-picked key subset.
             * This also covers z_level, exit/lane fields and future per-slot keys.
             */
            if (!canProcessManeuverIndex(i)) {
                continue;
            }

            if (d.has(p + "type")) {
                int v = d.num(p + "type", -1);
                if (v != state.mType[i]) {
                    state.mType[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "turn_angle")) {
                int v = d.num(p + "turn_angle", -1);
                if (v != state.mTurnAngle[i]) {
                    state.mTurnAngle[i] = v;
                    /*
                     * The native bridge publishes the iAP2 "exit angle" field under the
                     * legacy key name "turn_angle". Keep an explicit copy so side-streets
                     * mapping can use reference naming.
                     */
                    state.mExitAngle[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "z_level")) {
                int v = d.num(p + "z_level", -1);
                if (v != state.mZLevel[i]) {
                    state.mZLevel[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "zlevel")) {
                int v = d.num(p + "zlevel", -1);
                if (v != state.mZLevel[i]) {
                    state.mZLevel[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "junction_type")) {
                int v = d.num(p + "junction_type", -1);
                if (v != state.mJunctionType[i]) {
                    state.mJunctionType[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "driving_side")) {
                int v = d.num(p + "driving_side", -1);
                if (v != state.mDrivingSide[i]) {
                    state.mDrivingSide[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "distance")) {
                int v = d.num(p + "distance", -1);
                if (v != state.mDistance[i]) {
                    state.mDistance[i] = v;
                    /*
                     * The reference renderer computes bargraph in the maneuver-update path (updateCurrentManeuver)
                     * using distanceBetweenManeuver. Trigger a distance+bargraph refresh when this changes.
                     */
                    state.markDirty(AltCoreRouteState.DIRTY_DIST_MAN);
                }
            }
            if (d.has(p + "name")) {
                String v = d.str(p + "name");
                if (!strEq(state.mName[i], v)) {
                    state.mName[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_TEXT);
                }
            }
            if (d.has(p + "after_road")) {
                String v = d.str(p + "after_road");
                if (!strEq(state.mAfterRoad[i], v)) {
                    state.mAfterRoad[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_TEXT);
                }
            }
            if (d.has(p + "exit_info")) {
                String v = d.str(p + "exit_info");
                if (!strEq(state.mExitInfo[i], v)) {
                    state.mExitInfo[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_TEXT);
                }
            }
            if (d.has(p + "junction_angles")) {
                int[] newAngles = d.intList(p + "junction_angles");
                if (!intArrayEq(state.mJunctionAngles[i], newAngles)) {
                    state.mJunctionAngles[i] = newAngles;
                    /* Side streets influence the maneuver rendering. */
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "exit_angle")) {
                int v = d.num(p + "exit_angle", 1000);
                if (v != state.mExitAngle[i]) {
                    state.mExitAngle[i] = v;
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_ICON);
                }
            }
            if (d.has(p + "ver")) {
                int v = d.num(p + "ver", -1);
                if (v != state.mVer[i]) {
                    state.mVer[i] = v;
                    /* Slot was reassigned to a different iOS maneuver.
                     * Force full icon+text refresh even if type/angles are identical. */
                    state.markManeuverDirty(i, AltCoreRouteState.MAN_DIR_ICON | AltCoreRouteState.MAN_DIR_TEXT);
                }
            }
            if (d.has(p + "lane_count")) {
                int v = d.num(p + "lane_count", -1);
                if (v != state.mLaneCount[i]) {
                    state.mLaneCount[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "linked_lane_guidance_index")) {
                int v = d.num(p + "linked_lane_guidance_index", -1);
                if (v != state.mLinkedLaneGuidanceIndex[i]) {
                    state.mLinkedLaneGuidanceIndex[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "linked_lane_guidance_slot")) {
                int v = d.num(p + "linked_lane_guidance_slot", -1);
                if (v != state.mLinkedLaneGuidanceSlot[i]) {
                    state.mLinkedLaneGuidanceSlot[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_positions")) {
                int[] v = d.intList(p + "lane_positions");
                if (!intArrayEq(state.mLanePositions[i], v)) {
                    state.mLanePositions[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_directions")) {
                int[] v = d.intList(p + "lane_directions");
                if (!intArrayEq(state.mLaneDirections[i], v)) {
                    state.mLaneDirections[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_status")) {
                int[] v = d.intList(p + "lane_status");
                if (!intArrayEq(state.mLaneStatus[i], v)) {
                    state.mLaneStatus[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
            if (d.has(p + "lane_angles")) {
                int[][] v = parseLaneAnglesMatrix(d.str(p + "lane_angles", null));
                if (!intMatrixEq(state.mLaneAngles[i], v)) {
                    state.mLaneAngles[i] = v;
                    state.markDirty(AltCoreRouteState.DIRTY_LANE_GUIDANCE);
                }
            }
        }

        logger.debug( "Bus update: mask=0x" + Integer.toHexString(state.dirtyMask) +
              " keys=" + d.size() + " [" + dirtyToString(state) + "]" +
              " count=" + state.maneuverCount + " dist=" + state.distManeuverM + "m");
    }

    /**
     * Match dio_manager CRouteGuidanceUpdateProcessorImpl::isRouteGuidanceManeuverIndexGreater.
     */
    private boolean canProcessManeuverIndex(int idx) {
        if (!hasRouteUpdate) {
            return false;
        }
        /* Storage and presentation are intentionally separate.  A real 0x5202 may preload a future
         * maneuver before 0x5201 supplies an authoritative maneuver_list.  Cache the slot
         * fields now; BAPBridge still presents only slots selected by maneuverOrder. */
        return true;
    }

    private static boolean strEq(String a, String b) {
        if (a == null) return b == null;
        return a.equals(b);
    }

    private static boolean intArrayEq(int[] a, int[] b) {
        if (a == b) return true;
        if (a == null || b == null) return false;
        if (a.length != b.length) return false;
        for (int i = 0; i < a.length; i++) {
            if (a[i] != b[i]) return false;
        }
        return true;
    }

    private static boolean intMatrixEq(int[][] a, int[][] b) {
        if (a == b) return true;
        if (a == null || b == null) return false;
        if (a.length != b.length) return false;
        for (int i = 0; i < a.length; i++) {
            if (!intArrayEq(a[i], b[i])) return false;
        }
        return true;
    }

    private static int[][] parseLaneAnglesMatrix(String raw) {
        if (raw == null || raw.length() == 0) return null;

        int lanes = 1;
        for (int i = 0; i < raw.length(); i++) {
            if (raw.charAt(i) == '|') lanes++;
        }

        int[][] out = new int[lanes][];
        int start = 0;
        int lane = 0;
        for (int i = 0; i <= raw.length() && lane < lanes; i++) {
            if (i == raw.length() || raw.charAt(i) == '|') {
                out[lane++] = parseCsvIntList(raw.substring(start, i));
                start = i + 1;
            }
        }
        return out;
    }

    private static int[] parseCsvIntList(String s) {
        if (s == null) return null;
        s = s.trim();
        if (s.length() == 0) return new int[0];

        int commas = 0;
        for (int i = 0; i < s.length(); i++) {
            if (s.charAt(i) == ',') commas++;
        }

        int[] out = new int[commas + 1];
        int start = 0;
        int idx = 0;
        for (int i = 0; i <= s.length() && idx < out.length; i++) {
            if (i == s.length() || s.charAt(i) == ',') {
                String part = s.substring(start, i).trim();
                if (part.length() == 0) return null;
                try {
                    out[idx++] = Integer.parseInt(part);
                } catch (Exception e) {
                    return null;
                }
                start = i + 1;
            }
        }
        return out;
    }

    private static String dirtyToString(AltCoreRouteState s) {
        StringBuffer sb = new StringBuffer();
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_ROUTE_STATE) != 0, "route_state");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_MANEUVER_STATE) != 0, "maneuver_state");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_MANEUVER_COUNT) != 0, "maneuver_count");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_MANEUVER_LIST) != 0, "maneuver_list");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_DIST_DEST) != 0, "dist_dest_m");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_DIST_MAN) != 0, "dist_maneuver_m");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_ETA) != 0, "eta_seconds");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_TIME_REMAINING) != 0, "time_remaining");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_CURRENT_ROAD) != 0, "current_road");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_DESTINATION) != 0, "destination");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_DISCONNECT) != 0, "disconnect_reason");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_VISIBLE_IN_APP) != 0, "visible_in_app");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_SOURCE_SUPPORTS_RG) != 0, "source_supports_rg");
        appendIf(sb, (s.dirtyMask & AltCoreRouteState.DIRTY_LANE_GUIDANCE) != 0, "lane_guidance");

        boolean firstMan = true;
        for (int i = 0; i < MAX_MANEUVERS; i++) {
            int md = s.mDirty[i];
            if (md == 0) continue;
            if (firstMan) {
                appendSep(sb);
                sb.append("maneuver=");
                firstMan = false;
            } else {
                sb.append(',');
            }
            sb.append(i);
            sb.append(':');
            if ((md & AltCoreRouteState.MAN_DIR_ICON) != 0) sb.append('I');
            if ((md & AltCoreRouteState.MAN_DIR_TEXT) != 0) sb.append('T');
        }

        return sb.toString();
    }

    private static void appendIf(StringBuffer sb, boolean cond, String label) {
        if (!cond) return;
        appendSep(sb);
        sb.append(label);
    }

    private static void appendSep(StringBuffer sb) {
        if (sb.length() > 0) sb.append(',');
    }
}
