package com.luka.carplay.routeguidance;

/** Shared CarPlay navigation state. No platform service references.
 * Firmware input connectors normalize events into this state; output connectors
 * may use its dirty flags without owning the state machine. */
public class AltCoreRouteState {
    private static final int MAX_MANEUVERS=32;

        /* Dirty flags (top-level) */
        public static final int DIRTY_ROUTE_STATE      = 1 << 0;
        public static final int DIRTY_MANEUVER_STATE   = 1 << 1;
        public static final int DIRTY_MANEUVER_COUNT   = 1 << 2;
        public static final int DIRTY_MANEUVER_LIST    = 1 << 3;
        public static final int DIRTY_DIST_DEST        = 1 << 4;
        public static final int DIRTY_DIST_MAN         = 1 << 7;
        public static final int DIRTY_ETA              = 1 << 10;
        public static final int DIRTY_TIME_REMAINING   = 1 << 11;
        public static final int DIRTY_CURRENT_ROAD     = 1 << 12;
        public static final int DIRTY_DESTINATION      = 1 << 13;
        public static final int DIRTY_DISCONNECT       = 1 << 14;
        public static final int DIRTY_MANEUVER_ICON    = 1 << 15;
        public static final int DIRTY_MANEUVER_TEXT    = 1 << 16;
        public static final int DIRTY_VISIBLE_IN_APP   = 1 << 17;
        public static final int DIRTY_SOURCE_SUPPORTS_RG = 1 << 20;
        public static final int DIRTY_LANE_GUIDANCE    = 1 << 21;

        /* Per-maneuver dirty */
        public static final int MAN_DIR_ICON = 1;
        public static final int MAN_DIR_TEXT = 2;

        public int dirtyMask = 0;
        public int[] mDirty = new int[MAX_MANEUVERS];

        /* Route */
        public int routeState = -1;
        public int maneuverState = -1;
        public int maneuverCount = 0;
        public int[] maneuverOrder = null;
        public int visibleInApp = -1;
        public int sourceSupportsRg = -1;
        public int laneGuidanceShowing = -1;
        public int laneGuidanceTotal = -1;
        public int laneGuidanceIndex = -1;
        public int laneGuidanceSlot = -1;

        /* Distance to destination */
        public int distDestM = -1;

        /* Distance to maneuver */
        public int distManeuverM = -1;

        /* Time */
        public int etaSeconds = -1;
        public long timeRemainingSeconds = -1;

        /* Roads */
        public String currentRoad = null;
        public String destination = null;

        /* Disconnect reason */
        public String disconnectReason = null;

        /* Maneuvers */
        public int[] mType = new int[MAX_MANEUVERS];
        public int[] mTurnAngle = new int[MAX_MANEUVERS];
        /* Z-LevelGuidance: 0=none, 1=up, 2=down, -1=unknown */
        public int[] mZLevel = new int[MAX_MANEUVERS];
        public int[] mJunctionType = new int[MAX_MANEUVERS];
        public int[] mDrivingSide = new int[MAX_MANEUVERS];
        public int[] mDistance = new int[MAX_MANEUVERS];
        public String[] mName = new String[MAX_MANEUVERS];
        public String[] mAfterRoad = new String[MAX_MANEUVERS];
        public String[] mExitInfo = new String[MAX_MANEUVERS];
        public int[][] mJunctionAngles = new int[MAX_MANEUVERS][];
        /*
         * Junction element exit angle in degrees.
         * The reference route-guidance processor uses sentinel 1000 when the field is missing.
         */
        public int[] mExitAngle = new int[MAX_MANEUVERS];
        public int[] mLinkedLaneGuidanceIndex = new int[MAX_MANEUVERS];
        public int[] mLinkedLaneGuidanceSlot = new int[MAX_MANEUVERS];
        /* Slot version: bumps in C hook when a different iOS maneuver is assigned to a slot.
         * Detects maneuver transitions even when type/angles are identical. */
        public int[] mVer = new int[MAX_MANEUVERS];

        public int[][] mLanePositions = new int[MAX_MANEUVERS][];
        public int[] mLaneCount = new int[MAX_MANEUVERS];
        public int[][] mLaneDirections = new int[MAX_MANEUVERS][];
        public int[][] mLaneStatus = new int[MAX_MANEUVERS][];
        /*
         * Per-lane angle vectors from 0x5204 lane informations.
         * Format on the bus: lane0 comma-list, lanes separated by '|'
         * Example: "1000|40,20|"
         */
        public int[][][] mLaneAngles = new int[MAX_MANEUVERS][][];

        /*
         * Lane-guidance cache keyed by iOS composedGuidanceEventIndex.
         * These lg* arrays are separate from maneuver slots: CarPlay can send
         * lane event 0 while maneuver slot 0 currently belongs to iOS
         * maneuver 2 after reconnect/reroute.
         */
        public int[] lgIndex = new int[MAX_MANEUVERS];
        public int[][] lgLanePositions = new int[MAX_MANEUVERS][];
        public int[] lgLaneCount = new int[MAX_MANEUVERS];
        public int[][] lgLaneDirections = new int[MAX_MANEUVERS][];
        public int[][] lgLaneStatus = new int[MAX_MANEUVERS][];
        public int[][][] lgLaneAngles = new int[MAX_MANEUVERS][][];

        public AltCoreRouteState() {
            reset();
        }

        public void reset() {
            routeState = -1;
            maneuverState = -1;
            maneuverCount = 0;
            maneuverOrder = null;
            distDestM = -1;
            distManeuverM = -1;
            etaSeconds = -1;
            timeRemainingSeconds = -1;
            currentRoad = null;
            destination = null;
            disconnectReason = null;
            visibleInApp = -1;
            sourceSupportsRg = -1;
            laneGuidanceShowing = -1;
            laneGuidanceTotal = -1;
            laneGuidanceIndex = -1;
            laneGuidanceSlot = -1;
            clearAllManeuverSlots();
            clearDirty();
        }

        /* Reset per-maneuver and per-lane-cache arrays to their initial
         * empty values without touching dirty flags or top-level fields.
         * Shared between full reset() and the source_supports_rg=0
         * hard-clear path in parse(). */
        public void clearAllManeuverSlots() {
            for (int i = 0; i < MAX_MANEUVERS; i++) {
                mType[i] = -1;
                mTurnAngle[i] = -1;
                mZLevel[i] = -1;
                mJunctionType[i] = -1;
                mDrivingSide[i] = -1;
                mDistance[i] = -1;
                mName[i] = null;
                mAfterRoad[i] = null;
                mExitInfo[i] = null;
                mJunctionAngles[i] = null;
                mExitAngle[i] = 1000;
                mVer[i] = -1;
                mLinkedLaneGuidanceIndex[i] = -1;
                mLinkedLaneGuidanceSlot[i] = -1;
                mLanePositions[i] = null;
                mLaneCount[i] = -1;
                mLaneDirections[i] = null;
                mLaneStatus[i] = null;
                mLaneAngles[i] = null;
                lgIndex[i] = -1;
                lgLanePositions[i] = null;
                lgLaneCount[i] = -1;
                lgLaneDirections[i] = null;
                lgLaneStatus[i] = null;
                lgLaneAngles[i] = null;
            }
        }

        public void clearDirty() {
            dirtyMask = 0;
            for (int i = 0; i < MAX_MANEUVERS; i++) {
                mDirty[i] = 0;
            }
        }

        public void markDirty(int flag) {
            dirtyMask |= flag;
        }

        public void markManeuverDirty(int idx, int flag) {
            if (idx < 0 || idx >= MAX_MANEUVERS) return;
            mDirty[idx] |= flag;
            if ((flag & MAN_DIR_ICON) != 0) dirtyMask |= DIRTY_MANEUVER_ICON;
            if ((flag & MAN_DIR_TEXT) != 0) dirtyMask |= DIRTY_MANEUVER_TEXT;
        }

    
}
