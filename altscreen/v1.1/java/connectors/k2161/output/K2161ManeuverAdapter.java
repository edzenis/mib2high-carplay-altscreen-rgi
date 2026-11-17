package com.luka.carplay.routeguidance;

/** Firmware adapter: converts the K2161 route cache into a core-only semantic snapshot.
 * All firmware-specific maneuver encoding assumptions live here. */
final class K2161ManeuverAdapter {
    private K2161ManeuverAdapter() { }
    private static AltCoreManeuverPolicy.Snapshot snapshot(AltCoreRouteState s) {
        if (s == null) return null;
        return new AltCoreManeuverPolicy.Snapshot(
            s.mType, s.mDistance, s.maneuverOrder, s.distManeuverM,
            ManeuverMapper.MT_NO_TURN, ManeuverMapper.MT_CHANGE_HIGHWAY_RIGHT,
            ManeuverMapper.MT_START_ROUTE);
    }
    static int getFirstManeuverIndex(AltCoreRouteState s) {
        return AltCoreManeuverPolicy.getFirstManeuverIndex(snapshot(s));
    }
    static int getAuthoritativeHeadManeuverIndex(AltCoreRouteState s) {
        return AltCoreManeuverPolicy.getAuthoritativeHeadManeuverIndex(snapshot(s));
    }
    static int[] getManeuverIndexList(AltCoreRouteState s) {
        return AltCoreManeuverPolicy.getManeuverIndexList(snapshot(s));
    }
    static AltCoreManeuverPolicy.PresentationManeuver resolvePresentationManeuver(AltCoreRouteState s) {
        return AltCoreManeuverPolicy.resolvePresentationManeuver(snapshot(s));
    }
    static int getActionThresholdM(AltCoreRouteState s, int index, int threshold) {
        return AltCoreManeuverPolicy.getActionThresholdM(snapshot(s), index, threshold);
    }
}
