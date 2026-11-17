package com.luka.carplay.routeguidance;

/** AltScreen firmware-independent navigation presentation policy.
 * Accepts an immutable-at-call-site normalized snapshot from firmware connectors.
 * Does not depend on any vehicle API or connector class. */
final class AltCoreManeuverPolicy {
    private AltCoreManeuverPolicy() { }
    /* Values are normalized by the input connector. They are semantic data,
     * not a device/Java service type. The arrays are read-only during a call. */
    static final class Snapshot {
        final int[] mType, mDistance, maneuverOrder;
        final int distManeuverM, validTypeMin, validTypeMax, startRouteType;
        Snapshot(int[] types, int[] distances, int[] order, int distance,
                 int minType, int maxType, int startType) {
            mType=types; mDistance=distances; maneuverOrder=order;
            distManeuverM=distance; validTypeMin=minType; validTypeMax=maxType;
            startRouteType=startType;
        }
    }
    private static boolean valid(Snapshot s, int type) {
        return type >= s.validTypeMin && type <= s.validTypeMax;
    }
    private static final int PRESENTATION_DISTANCE_TOLERANCE_M = 100;
    private static final int ACTION_PERCENT_OF_PREPARE = 15;

    static final class PresentationManeuver {

        final int index;
        final int type;
        final int distanceM;
        final int liveDistanceM;
        final int slotDistanceM;
        final boolean skippedStartRoute;
        final boolean usedSlotDistance;

        PresentationManeuver(int index, int type, int distanceM,
                             int liveDistanceM, int slotDistanceM,
                             boolean skippedStartRoute, boolean usedSlotDistance) {
            this.index = index;
            this.type = type;
            this.distanceM = distanceM;
            this.liveDistanceM = liveDistanceM;
            this.slotDistanceM = slotDistanceM;
            this.skippedStartRoute = skippedStartRoute;
            this.usedSlotDistance = usedSlotDistance;
        }
        }

    static int getAuthoritativeHeadManeuverIndex(Snapshot s) {
        int maxIdx = (s != null && s.mType != null) ? s.mType.length : 0;
        if (s == null || s.maneuverOrder == null || s.maneuverOrder.length == 0) return -1;

        for (int i = 0; i < s.maneuverOrder.length; i++) {
            int idx = s.maneuverOrder[i];
            if (idx >= 0 && idx < maxIdx && valid(s, s.mType[idx])) {
                return idx;
            }
        }
        return -1;
    }

    static PresentationManeuver resolvePresentationManeuver(Snapshot s) {
        int idx = getFirstManeuverIndex(s);
        int type = (idx >= 0 && s != null && s.mType != null && idx < s.mType.length)
            ? s.mType[idx] : -1;
        int liveDist = (s != null) ? s.distManeuverM : -1;
        int slotDist = (idx >= 0 && s != null && s.mDistance != null && idx < s.mDistance.length)
            ? s.mDistance[idx] : -1;
        int resolvedDist = liveDist;
        boolean skippedStartRoute = false;
        boolean usedSlotDistance = false;

        int headIdx = getAuthoritativeHeadManeuverIndex(s);
        if (headIdx >= 0 && idx >= 0 && headIdx != idx
                && s.mType[headIdx] == s.startRouteType) {
            skippedStartRoute = true;
            if (slotDist > 0) {
                long maxPlausibleLive = (long) slotDist + PRESENTATION_DISTANCE_TOLERANCE_M;
                if (liveDist <= 0 || (long) liveDist > maxPlausibleLive) {
                    resolvedDist = slotDist;
                    usedSlotDistance = true;
                }
            }
        }

        return new PresentationManeuver(idx, type, resolvedDist, liveDist, slotDist,
            skippedStartRoute, usedSlotDistance);
    }

    static int getFirstManeuverIndex(Snapshot s) {
        int maxIdx = (s.mType != null) ? s.mType.length : 0;
        if (s.maneuverOrder == null || s.maneuverOrder.length == 0) return -1;

        int first = -1;
        int firstOrderPos = -1;
        for (int i = 0; i < s.maneuverOrder.length; i++) {
            int idx = s.maneuverOrder[i];
            if (idx >= 0 && idx < maxIdx && valid(s, s.mType[idx])) {
                first = idx;
                firstOrderPos = i;
                break;
            }
        }
        if (first < 0) return -1;

        /* iOS commonly puts START_ROUTE at the head of the authoritative list.
         * Keep it in the cache/order, but present the first real maneuver when one
         * is already available. */
        if (s.mType[first] == s.startRouteType) {
            for (int i = firstOrderPos + 1; i < s.maneuverOrder.length; i++) {
                int idx = s.maneuverOrder[i];
                if (idx >= 0 && idx < maxIdx
                        && valid(s, s.mType[idx])
                        && s.mType[idx] != s.startRouteType) {
                    return idx;
                }
            }
            return -1;
        }
        return first;
    }

    static int[] getManeuverIndexList(Snapshot s) {
        if (s.maneuverOrder != null && s.maneuverOrder.length == 0) {
            return null;
        }
        if (s.maneuverOrder != null && s.maneuverOrder.length > 0) {
            return s.maneuverOrder;
        }
        /*
         * Do not synthesize [0..maneuver_count) when iOS has not published
         * an explicit maneuver_list yet.  During reroute, count often arrives
         * before the new slot payloads; falling back to numeric slot order can
         * briefly expose stale pre-reroute slots to the navigation output path.
         */
        return null;
    }

    static int getActionThresholdM(Snapshot s, int manIdx, int prepareThresholdM) {
        if (manIdx < 0 || s == null || s.mDistance == null || manIdx >= s.mDistance.length) {
            return -1;
        }
        int policyCap = (prepareThresholdM * ACTION_PERCENT_OF_PREPARE) / 100;
        if (policyCap <= 0) return -1;
        int stepDistance = s.mDistance[manIdx];
        if (stepDistance <= 0 || stepDistance > policyCap) return policyCap;
        return stepDistance;
    }

}
