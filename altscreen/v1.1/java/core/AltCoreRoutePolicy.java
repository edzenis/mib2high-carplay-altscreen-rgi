package com.luka.carplay.routeguidance;

/** Shared Apple navigation ownership decision, with no OEM service/API calls. */
final class AltCoreRoutePolicy {
    private AltCoreRoutePolicy() { }
    static boolean wantsRouteOwnership(AltCoreRouteState candidate) {
        boolean explicitClear = ((candidate.dirtyMask & AltCoreRouteState.DIRTY_MANEUVER_COUNT) != 0)
            && (candidate.maneuverCount == 0);

        boolean routeLooksActive = (candidate.routeState >= 1)
            || (candidate.maneuverCount > 0)
            || (candidate.maneuverOrder != null && candidate.maneuverOrder.length > 0);

        boolean wantActive;
        if (candidate.visibleInApp >= 0) {
            wantActive = (candidate.visibleInApp != 0) || routeLooksActive;
        } else {
            wantActive = routeLooksActive;
            if (explicitClear && candidate.routeState < 1) {
                wantActive = false;
            }
        }

        if (candidate.sourceSupportsRg == 0) wantActive = false;
        if (candidate.routeState == 0) wantActive = false;
        return wantActive;
        }
}
