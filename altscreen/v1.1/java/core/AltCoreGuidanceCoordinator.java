package com.luka.carplay.routeguidance;
/** Single firmware-neutral owner of route-on/off decisions. */
public final class AltCoreGuidanceCoordinator {
    private boolean active;
    private long generation;
    public boolean isActive() { return active; }
    public long generation() { return generation; }
    public boolean evaluate(AltCoreRouteState s, boolean activationDelta) {
        if (activationDelta) active = AltCoreRoutePolicy.wantsRouteOwnership(s);
        return active;
    }
    public void reset() { active = false; generation++; }
}
